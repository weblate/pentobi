//-----------------------------------------------------------------------------
/** @file libpentobi_base/Piece.cpp */
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Piece.h"

#include <algorithm>
#include <boost/foreach.hpp>
#include "libboardgame_base/GeometryUtil.h"
#include "libboardgame_util/Assert.h"
#include "libboardgame_util/Log.h"

namespace libpentobi_base {

using namespace std;
using libboardgame_base::geometry_util::normalize_offset;
using libboardgame_base::geometry_util::type_match_shift;
using libboardgame_util::log;

//-----------------------------------------------------------------------------

namespace {

const bool log_piece_creation = false;

struct NormalizedPoints
{
    /** The normalized points of the transformed piece.
        The points were shifted using GeometryUtil::normalize_offset(). */
    Piece::Points points;

    /** The point type of (0,0) in the normalized points. */
    unsigned int point_type;

    bool operator==(const NormalizedPoints& n) const
    {
        return points == n.points && point_type == n.point_type;
    }
};

#if LIBBOARDGAME_DEBUG
/** Check consistency of transformations.
    Checks that the point list (which must be already sorted) has no
    duplicates. */
bool check_consistency(const Piece::Points& points)
{
    for (unsigned int i = 0; i < points.size(); ++i)
        if (i > 0 && points[i] == points[i - 1])
            return false;
    return true;
}
#endif // LIBBOARDGAME_DEBUG

/** Bring piece points into a normal form that is constant under translation. */
NormalizedPoints normalize(const Piece::Points& points, unsigned int point_type,
                           const Geometry& geometry)
{
    if (log_piece_creation)
        log() << "Points " << points << '\n';
    NormalizedPoints normalized;
    normalized.points = points;
    type_match_shift(geometry, normalized.points.begin(),
                     normalized.points.end(), point_type);
    if (log_piece_creation)
        log() << "Point type " << point_type << ", type match shift "
              << normalized.points << '\n';
    // Make the coordinates positive and minimal
    unsigned int width; // unused
    unsigned int height; // unused
    CoordPoint offset;
    normalize_offset(geometry, normalized.points.begin(),
                     normalized.points.end(), width, height, offset);
    normalized.point_type = geometry.get_point_type(offset);
    // Sort the coordinates
    sort(normalized.points.begin(), normalized.points.end());
    return normalized;
}

} // namespace

//-----------------------------------------------------------------------------

Piece::Piece(const string& name, const Piece::Points& points,
             const Geometry& geometry, const PieceTransforms& transforms)
    : m_name(name),
      m_points(points),
      m_transforms(&transforms)
{
    if (log_piece_creation)
        log() << "Creating transformations for piece " << name
              << ' ' << points << '\n';
    LIBBOARDGAME_ASSERT(points.contains(CoordPoint(0, 0)));
    vector<NormalizedPoints> all_transformed_points;
    Piece::Points transformed_points;
    BOOST_FOREACH(const Transform* transform, transforms.get_all())
    {
        if (log_piece_creation)
            log() << "Transformation " << typeid(*transform).name() << '\n';
        transformed_points = points;
        transform->transform(transformed_points.begin(),
                             transformed_points.end());
        NormalizedPoints normalized = normalize(transformed_points,
                                                transform->get_new_point_type(),
                                                geometry);
        if (log_piece_creation)
            log() << "Normalized " << normalized.points << " point type "
                  << normalized.point_type << '\n';
        LIBBOARDGAME_ASSERT(check_consistency(normalized.points));
        auto begin = all_transformed_points.begin();
        auto end = all_transformed_points.end();
        auto pos = find(begin, end, normalized);
        if (pos != end)
        {
            if (log_piece_creation)
                log() << "Equivalent to " << (pos - begin) << '\n';
            m_equivalent_transform[transform]
                = transforms.get_all()[pos - begin];
        }
        else
        {
            if (log_piece_creation)
                log() << "New (" << m_uniq_transforms.size() << ")\n";
            m_equivalent_transform[transform] = transform;
            m_uniq_transforms.push_back(transform);
        }
        all_transformed_points.push_back(normalized);
    };
}

bool Piece::can_flip_horizontally(const Transform* transform) const
{
    transform = get_equivalent_transform(transform);
    const Transform* flip = get_equivalent_transform(
                            m_transforms->get_mirrored_horizontally(transform));
    return flip != transform;
}

bool Piece::can_flip_vertically(const Transform* transform) const
{
    transform = get_equivalent_transform(transform);
    const Transform* flip = get_equivalent_transform(
                              m_transforms->get_mirrored_vertically(transform));
    return flip != transform;
}

bool Piece::can_rotate() const
{
    const Transform* transform = m_uniq_transforms[0];
    const Transform* rotate = get_equivalent_transform(
                                m_transforms->get_rotated_clockwise(transform));
    return rotate != transform;
}

const Transform* Piece::find_transform(const Geometry& geometry,
                                       const Points& points) const
{
    NormalizedPoints normalized =
        normalize(points, geometry.get_point_type(0, 0), geometry);
    BOOST_FOREACH(const Transform* transform, get_transforms())
    {
        Points piece_points = get_points();;
        transform->transform(piece_points.begin(), piece_points.end());
        NormalizedPoints normalized_piece =
            normalize(piece_points, transform->get_new_point_type(), geometry);
        if (normalized_piece == normalized)
            return transform;
    }
    return 0;
}

const Transform* Piece::get_equivalent_transform(
                                               const Transform* transform) const
{
    auto pos = m_equivalent_transform.find(transform);
    if (pos == m_equivalent_transform.end())
    {
        LIBBOARDGAME_ASSERT(false);
        return 0;
    }
    return pos->second;
}

const Transform* Piece::get_next_transform(const Transform* transform) const
{
    transform = get_equivalent_transform(transform);
    auto begin = m_uniq_transforms.begin();
    auto end = m_uniq_transforms.end();
    auto pos = find(begin, end, transform);
    LIBBOARDGAME_ASSERT(pos != end);
    if (pos + 1 == end)
        return *begin;
    else
        return *(pos + 1);
}

const Transform* Piece::get_previous_transform(const Transform* transform) const
{
    transform = get_equivalent_transform(transform);
    auto begin = m_uniq_transforms.begin();
    auto end = m_uniq_transforms.end();
    auto pos = find(begin, end, transform);
    LIBBOARDGAME_ASSERT(pos != end);
    if (pos == begin)
        return *(end - 1);
    else
        return *(pos - 1);
}

//-----------------------------------------------------------------------------

} // namespace libpentobi_base
