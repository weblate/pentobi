//-----------------------------------------------------------------------------
/** @file libpentobi_base/Tree.cpp */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Tree.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "BoardUpdater.h"
#include "BoardUtil.h"
#include "libboardgame_sgf/Util.h"
#include "libboardgame_util/StringUtil.h"

namespace libpentobi_base {

using boost::is_any_of;
using boost::split;
using boost::trim_copy;
using boost::algorithm::to_lower_copy;
using libboardgame_sgf::ChildIterator;
using libboardgame_sgf::InvalidPropertyValue;
using libboardgame_sgf::InvalidTree;
using libboardgame_sgf::PropertyIterator;
using libboardgame_sgf::util::get_go_point_property_value;
using libboardgame_sgf::util::parse_go_move_property_value;
using libboardgame_sgf::util::parse_go_point_property_value;
using libboardgame_util::string_util::to_string;
using libpentobi_base::boardutil::get_current_position_as_setup;

//-----------------------------------------------------------------------------

Tree::Tree(GameVariant game_variant)
{
    init_game_variant(game_variant);
}

Tree::Tree(unique_ptr<Node>& root)
{
    init(root);
}

void Tree::add_setup(const Node& node, const Setup& setup)
{
    switch (m_game_variant)
    {
    case game_variant_classic:
    case game_variant_classic_2:
    case game_variant_trigon:
    case game_variant_trigon_2:
        set_setup_property(node, "A1", setup.placements[Color(0)]);
        set_setup_property(node, "A2", setup.placements[Color(1)]);
        set_setup_property(node, "A3", setup.placements[Color(2)]);
        set_setup_property(node, "A4", setup.placements[Color(3)]);
        break;
    case game_variant_trigon_3:
        set_setup_property(node, "A1", setup.placements[Color(0)]);
        set_setup_property(node, "A2", setup.placements[Color(1)]);
        set_setup_property(node, "A3", setup.placements[Color(2)]);
        break;
    default:
        LIBBOARDGAME_ASSERT(m_game_variant == game_variant_duo);
        set_setup_property(node, "AB", setup.placements[Color(0)]);
        set_setup_property(node, "AW", setup.placements[Color(1)]);
    }
    switch (m_game_variant)
    {
    case game_variant_classic:
    case game_variant_classic_2:
    case game_variant_trigon:
    case game_variant_trigon_2:
    case game_variant_trigon_3:
        set_property(node, "PL", setup.to_play.to_int() + 1);
        break;
    default:
        LIBBOARDGAME_ASSERT(m_game_variant == game_variant_duo);
        set_property(node, "PL", setup.to_play == Color(0) ? "B" : "W");
    }
}

const Node* Tree::find_child_with_move(const Node& node, ColorMove mv) const
{
    for (ChildIterator i(node); i; ++i)
        if (get_move(*i) == mv)
            return &(*i);
    return 0;
}

bool Tree::get_move(const Node& node, GameVariant game_variant, Color& c,
                    MovePoints& points)
{
    string id;
    if (game_variant == game_variant_duo)
    {
        if (node.has_property("B"))
        {
            id = "B";
            c = Color(0);
        }
        else if (node.has_property("W"))
        {
            id = "W";
            c = Color(1);
        }
    }
    else
    {
        // Properties BLUE/YELLOW/RED/GREEN were used by Pentobi 0.1
        // Newer versions of Pentobi use 1/2/3/4 as suggested by SGF FF[5]
        if (node.has_property("1"))
        {
            id = "1";
            c = Color(0);
        }
        else if (node.has_property("2"))
        {
            id = "2";
            c = Color(1);
        }
        else if (node.has_property("3"))
        {
            id = "3";
            c = Color(2);
        }
        else if (node.has_property("4"))
        {
            id = "4";
            c = Color(3);
        }
        else if (node.has_property("BLUE"))
        {
            id = "BLUE";
            c = Color(0);
        }
        else if (node.has_property("YELLOW"))
        {
            id = "YELLOW";
            c = Color(1);
        }
        else if (node.has_property("RED"))
        {
            id = "RED";
            c = Color(2);
        }
        else if (node.has_property("GREEN"))
        {
            id = "GREEN";
            c = Color(3);
        }
    }
    if (id.empty())
        return false;
    vector<string> values;
    values = node.get_multi_property(id);
    // Note: we still support having the points of a move in a list of point
    // values instead of a single value as used by Pentobi <= 0.2, but it
    // is deprecated
    points.clear();
    BOOST_FOREACH(const string& s, values)
    {
        if (trim_copy(s).empty())
            continue;
        vector<string> v;
        split(v, s, is_any_of(","));
        BOOST_FOREACH(const string& p_str, v)
        {
            Point p;
            try
            {
                p = Point::from_string(p_str);
            }
            catch (const Point::InvalidString&)
            {
                throw InvalidPropertyValue(id, p_str);
            }
            points.push_back(p);
        }
    }
    return true;
}

ColorMove Tree::get_move(const Node& node) const
{
    Color c;
    MovePoints points;
    if (! get_move(node, m_game_variant, c, points))
        return ColorMove::null();
    if (points.size() == 0)
        return ColorMove(c, Move::pass());
    Move mv;
    if (! m_board_const->find_move(points, mv))
        throw InvalidTree(str(format("Illegal move %1%") % to_string(points)));
    return ColorMove(c, mv);
}

ColorMove Tree::get_move_ignore_invalid(const Node& node) const
{
    try
    {
        return get_move(node);
    }
    catch (const InvalidTree&)
    {
        return ColorMove::null();
    }
}

const Node* Tree::get_node_before_move_number(unsigned int move_number) const
{
    const Node* node = &get_root();
    unsigned int n = 0;
    while (node->has_children())
    {
        const Node& child = node->get_first_child();
        if (! get_move(child).is_null() && n++ == move_number)
                return node;
        node = &child;
    }
    return 0;
}

string Tree::get_player_name(Color c) const
{
    const Node& root = get_root();
    switch (m_game_variant)
    {
    case game_variant_classic:
    case game_variant_trigon:
    case game_variant_trigon_3:
        if (c == Color(0))
            return root.get_property("P1", "");
        if (c == Color(1))
            return root.get_property("P2", "");
        if (c == Color(2))
            return root.get_property("P3", "");
        if (c == Color(3))
            return root.get_property("P4", "");
        break;
    case game_variant_classic_2:
    case game_variant_trigon_2:
        if (c == Color(0) || c == Color(2))
            return root.get_property("PB", "");
        if (c == Color(1) || c == Color(3))
            return root.get_property("PW", "");
        break;
    case game_variant_duo:
        if (c == Color(0))
            return root.get_property("PB", "");
        if (c == Color(1))
            return root.get_property("PW", "");
        break;
    }
    LIBBOARDGAME_ASSERT(false);
    return "";
}

bool Tree::has_main_variation_moves() const
{
    const Node* node = &get_root();
    while (node != 0)
    {
        if (has_move_ignore_invalid(*node))
            return true;
        node = node->get_first_child_or_null();
    }
    return false;
}

bool Tree::has_setup_properties(const Node& node)
{
    for (PropertyIterator i(node); i; ++i)
        // Don't check for "PL", we ignore this property if the node has no
        // other setup property, it doesn't make much sense and might only
        // confuse the user if suddenly the color to play changes in the GUI
        if (i->id == "AB" || i->id == "AW" || i->id == "A1" || i->id == "A2"
            || i->id == "A3" || i->id == "A4" || i->id == "AE")
            return true;
    return false;
}

void Tree::init(unique_ptr<Node>& root)
{
    GameVariant game_variant;
    string game = root->get_property("GM");
    string s = to_lower_copy(trim_copy(game));
    if (s == "blokus")
        game_variant = game_variant_classic;
    else if (s == "blokus two-player")
        game_variant = game_variant_classic_2;
    else if (s == "blokus trigon")
        game_variant = game_variant_trigon;
    else if (s == "blokus trigon two-player")
        game_variant = game_variant_trigon_2;
    else if (s == "blokus trigon three-player")
        game_variant = game_variant_trigon_3;
    else if (s == "blokus duo")
        game_variant = game_variant_duo;
    else
        throw InvalidPropertyValue("GM", game);
    libboardgame_sgf::Tree::init(root);
    m_game_variant = game_variant;
    init_board_const(game_variant);
}

void Tree::init_board_const(GameVariant game_variant)
{
    BoardType board_type;
    switch (game_variant)
    {
    case game_variant_classic:
    case game_variant_classic_2:
        board_type = board_type_classic;
        break;
    case game_variant_trigon:
    case game_variant_trigon_2:
        board_type = board_type_trigon;
        break;
    case game_variant_trigon_3:
        board_type = board_type_trigon_3;
        break;
    default:
        LIBBOARDGAME_ASSERT(game_variant == game_variant_duo);
        board_type = board_type_duo;
    }
    m_board_const = &BoardConst::get(board_type);
}

void Tree::init_game_variant(GameVariant game_variant)
{
    libboardgame_sgf::Tree::init();
    m_game_variant = game_variant;
    set_game_property();
    init_board_const(game_variant);
    clear_modified();
}

void Tree::keep_only_position(const Node& node)
{
    LIBBOARDGAME_ASSERT(contains(node));
    if (&node == &get_root())
        return;
    bool create_new_setup = has_move(node);
    if (! create_new_setup)
    {
        const Node* current = node.get_parent_or_null();
        while (current != 0)
        {
            if (has_move(*current) || has_setup_properties(*current))
            {
                create_new_setup = true;
                break;
            }
            current = current->get_parent_or_null();
        }
    }
    if (create_new_setup)
    {
        unique_ptr<Board> bd(new Board(m_game_variant));
        BoardUpdater updater(*this, *bd);
        updater.update(node);
        Setup setup;
        get_current_position_as_setup(*bd, setup);
        remove_property(node, "B");
        remove_property(node, "W");
        remove_property(node, "1");
        remove_property(node, "2");
        remove_property(node, "3");
        remove_property(node, "4");
        remove_property(node, "AB");
        remove_property(node, "AW");
        remove_property(node, "A1");
        remove_property(node, "A2");
        remove_property(node, "A3");
        remove_property(node, "A4");
        remove_property(node, "AE");
        LIBBOARDGAME_ASSERT(! has_move(node));
        LIBBOARDGAME_ASSERT(! has_setup_properties(node));
        add_setup(node, setup);
    }
    make_root(node);
    set_game_property();
}

void Tree::set_game_property()
{
    const Node& root = get_root();
    switch (m_game_variant)
    {
    case game_variant_classic:
        set_property(root, "GM", "Blokus");
        break;
    case game_variant_classic_2:
        set_property(root, "GM", "Blokus Two-Player");
        break;
    case game_variant_trigon:
        set_property(root, "GM", "Blokus Trigon");
        break;
    case game_variant_trigon_2:
        set_property(root, "GM", "Blokus Trigon Two-Player");
        break;
    case game_variant_trigon_3:
        set_property(root, "GM", "Blokus Trigon Three-Player");
        break;
    default:
        LIBBOARDGAME_ASSERT(m_game_variant == game_variant_duo);
        set_property(root, "GM", "Blokus Duo");
    }
}

void Tree::set_move(const Node& node, Color c, Move mv)
{
    string id;
    if (m_game_variant == game_variant_duo)
        id = (c == Color(0) ? "B" : "W");
    else
    {
        if (c == Color(0))
            id = "1";
        else if (c == Color(1))
            id = "2";
        else if (c == Color(2))
            id = "3";
        else if (c == Color(3))
            id = "4";
    }
    if (! mv.is_pass())
        set_property(node, id, m_board_const->to_string(mv, false));
    else
        set_property(node, id, "");
}

void Tree::set_player_name(Color c, const string& name)
{
    const Node& root = get_root();
    switch (m_game_variant)
    {
    case game_variant_classic:
    case game_variant_trigon:
    case game_variant_trigon_3:
        if (c == Color(0))
            set_property(root, "P1", name);
        else if (c == Color(1))
            set_property(root, "P2", name);
        else if (c == Color(2))
            set_property(root, "P3", name);
        else if (c == Color(3))
            set_property(root, "P4", name);
        else
            LIBBOARDGAME_ASSERT(false);
        return;
    case game_variant_classic_2:
    case game_variant_trigon_2:
        if (c == Color(0) || c == Color(2))
            set_property(root, "PB", name);
        else if (c == Color(1) || c == Color(3))
            set_property(root, "PW", name);
        else
            LIBBOARDGAME_ASSERT(false);
        return;
    case game_variant_duo:
        if (c == Color(0))
            set_property(root, "PB", name);
        else if (c == Color(1))
            set_property(root, "PW", name);
        else
            LIBBOARDGAME_ASSERT(false);
        return;
    }
    LIBBOARDGAME_ASSERT(false);
}

void Tree::set_result(const Node& node, int score)
{
    if (score > 0)
        set_property(node, "RE", format("B+%i") % score);
    else if (score < 0)
        set_property(node, "RE", format("W+%i") % -score);
    else
        set_property(node, "RE", "0");
}

void Tree::set_setup_property(const Node& node, const char* id,
                              const Setup::PlacementList& placements)
{
    if (placements.empty())
        return;
    vector<string> values;
    BOOST_FOREACH(Move mv, placements)
        values.push_back(m_board_const->to_string(mv, false));
    set_property(node, id, values);
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_base
