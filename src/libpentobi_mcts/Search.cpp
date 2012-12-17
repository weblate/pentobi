//-----------------------------------------------------------------------------
/** @file libpentobi_mcts/Search.cpp */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Search.h"

#include <boost/format.hpp>
#include "Util.h"

namespace libpentobi_mcts {

using boost::format;
using libboardgame_base::PointTransfRot180;
using libpentobi_base::BoardIterator;
using libpentobi_base::BoardType;
using libpentobi_base::ColorIterator;
using libpentobi_base::Piece;

//-----------------------------------------------------------------------------

namespace {

void filter_min_size(const BoardConst& board_const, unsigned min_size,
                     PieceMap<bool>& is_piece_considered)
{
    for (unsigned i = 0; i < board_const.get_nu_pieces(); ++i)
    {
        Piece piece(i);
        const PieceInfo& piece_info = board_const.get_piece_info(piece);
        if (piece_info.get_size() < min_size)
            is_piece_considered[piece] = false;
    }
}

void set_piece_considered(const BoardConst& board_const, const char* name,
                          PieceMap<bool>& is_piece_considered,
                          bool is_considered = true)
{
    Piece piece;
    bool found = board_const.get_piece_by_name(name, piece);
    LIBBOARDGAME_UNUSED_IF_NOT_DEBUG(found);
    LIBBOARDGAME_ASSERT(found);
    is_piece_considered[piece] = is_considered;
}

void set_pieces_considered(const Board& bd, unsigned nu_moves,
                           PieceMap<bool>& is_piece_considered)
{
    const BoardConst& board_const = bd.get_board_const();
    BoardType board_type = board_const.get_board_type();
    unsigned nu_colors = bd.get_nu_colors();
    is_piece_considered.fill(true);
    if (board_type == BoardType::duo)
    {
        if (nu_moves < 2 * nu_colors)
            filter_min_size(board_const, 5, is_piece_considered);
        else if (nu_moves < 3 * nu_colors)
            filter_min_size(board_const, 4, is_piece_considered);
        else if (nu_moves < 5 * nu_colors)
            filter_min_size(board_const, 3, is_piece_considered);
    }
    else if (board_type == BoardType::classic)
    {
        if (nu_moves < 1 * nu_colors)
        {
            is_piece_considered.fill(false);
            set_piece_considered(board_const, "V5", is_piece_considered);
            set_piece_considered(board_const, "Z5", is_piece_considered);
        }
        else if (nu_moves < 3 * nu_colors)
            filter_min_size(board_const, 5, is_piece_considered);
        else if (nu_moves < 5 * nu_colors)
            filter_min_size(board_const, 4, is_piece_considered);
        else if (nu_moves < 7 * nu_colors)
            filter_min_size(board_const, 3, is_piece_considered);
    }
    else if (board_type == BoardType::trigon
             || board_type == BoardType::trigon_3)
    {
        if (nu_moves < 1 * nu_colors)
        {
            is_piece_considered.fill(false);
            set_piece_considered(board_const, "V", is_piece_considered);
            // I5 would also be good but the distance to center pruning in
            // State would prune them anyway
        }
        if (nu_moves < 4 * nu_colors)
        {
            filter_min_size(board_const, 6, is_piece_considered);
            // O is a bad early move, it neither extends, nor blocks well
            set_piece_considered(board_const, "O", is_piece_considered, false);
        }
        else if (nu_moves < 5 * nu_colors)
            filter_min_size(board_const, 5, is_piece_considered);
        else if (nu_moves < 7 * nu_colors)
            filter_min_size(board_const, 4, is_piece_considered);
        else if (nu_moves < 9 * nu_colors)
            filter_min_size(board_const, 3, is_piece_considered);
    }
}

} // namespace

//-----------------------------------------------------------------------------

Search::Search(Variant initial_variant, unsigned nu_threads, size_t memory)
    : ParentClass(nu_threads == 0 ? util::get_nu_threads() : nu_threads,
                  memory == 0 ? util::get_memory() : memory),
      m_auto_param(true),
      m_variant(initial_variant),
      m_shared_const(m_to_play)
{
    set_rave(true);
    set_expand_threshold(3);
    set_last_good_reply(true);
    set_default_param(m_variant);
    create_threads();
}

Search::~Search() throw()
{
}

bool Search::check_followup(vector<Move>& sequence)
{
    m_state.init(get_board(), m_to_play);
    bool is_followup = m_state.is_followup(m_last_state, sequence);
    m_last_state = m_state;
    return is_followup;
}

unique_ptr<State> Search::create_state()
{
    return unique_ptr<State>(new State(m_variant, m_shared_const));
}

string Search::get_move_string(Move mv) const
{
    return get_board().to_string(mv);
}

void Search::get_root_position(Variant& variant, Setup& setup) const
{
    m_last_state.get_as_setup(variant, setup);
    setup.to_play = m_to_play;
}

Float Search::get_tie_value() const
{
    return 0.5;
}

void Search::on_start_search()
{
    const Board& bd = get_board();

    for (ColorIterator i(bd.get_nu_colors()); i; ++i)
    {
        MoveMarker& is_forbidden_at_root =
            m_shared_const.is_forbidden_at_root[*i];
        is_forbidden_at_root.set_all();
        for (BoardIterator j(bd); j; ++j)
            if (! bd.is_forbidden(*j, *i))
            {
                unsigned adj_status = bd.get_adj_status(*j, *i);
                for (Piece piece : bd.get_pieces_left(*i))
                {
                    for (Move mv : bd.get_moves(piece, *j, adj_status))
                    {
                        if (! is_forbidden_at_root[mv])
                            continue;
                        if (! bd.is_forbidden(*i, mv))
                            is_forbidden_at_root.clear(mv);
                    }
                }
            }
    }

    for (unsigned i = 0; i < Board::max_game_moves; ++i)
        set_pieces_considered(bd, i, m_shared_const.is_piece_considered[i]);
    m_shared_const.is_piece_considered_all.fill(true);

    PointTransfRot180<Point> transform;
    m_shared_const.symmetric_points.init(bd.get_geometry(), transform);
}

bool Search::search(Move& mv, const Board& bd, Color to_play,
                    Float max_count, size_t min_simulations,
                    double max_time, TimeSource& time_source)
{
    m_shared_const.board = &bd;
    m_to_play = to_play;
    Variant variant = bd.get_variant();
    if (m_auto_param && variant != m_variant)
        set_default_param(variant);
    m_variant = variant;
    bool result = ParentClass::search(mv, max_count, min_simulations, max_time,
                                      time_source, 0);
    return result;
}

void Search::set_default_param(Variant variant)
{
    log() << "Setting default parameters for " << to_string(variant)
          << '\n';
    switch (variant)
    {
    case Variant::duo:
        set_bias_term_constant(0.09f);
        break;
    case Variant::junior:
        set_bias_term_constant(0.12f);
        break;
    case Variant::classic:
        // Not tuned. Use same value as for Variant::classic_2
        set_bias_term_constant(0.11f);
        break;
    case Variant::classic_2:
        set_bias_term_constant(0.11f);
        break;
    case Variant::trigon:
        // Not tuned. Use same value as for Variant::trigon_2
        set_bias_term_constant(0.10f);
        break;
    case Variant::trigon_2:
        set_bias_term_constant(0.10f);
        break;
    case Variant::trigon_3:
        // Not tuned. Use same value as for Variant::trigon_2
        set_bias_term_constant(0.10f);
        break;
    default:
        LIBBOARDGAME_ASSERT(false);
        set_bias_term_constant(0.10f);
        break;
    }
}

void Search::write_info(ostream& out) const
{
    if (get_nu_simulations() == 0)
        return;
    const Node& root = get_tree().get_root();
    if (! root.has_children())
        return;
    ParentClass::write_info(out);
    out << (format("Mov: %i, ") % root.get_nu_children());
    if (libpentobi_base::get_nu_players(m_variant) > 2)
    {
        out << "All:";
        for (unsigned i = 0; i < libpentobi_base::get_nu_colors(m_variant); ++i)
        {
            if (get_root_val()[i].get_count() == 0)
                out << " -";
            else
                out << (format(" %.2f") % get_root_val()[i].get_mean());
        }
        out << ", ";
    }
    get_state(0).write_info(out);
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_mcts
