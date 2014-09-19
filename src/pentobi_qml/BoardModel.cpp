//-----------------------------------------------------------------------------
/** @file pentobi_qml/BoardModel.cpp
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#include "BoardModel.h"

#include <limits>
#include <QDebug>
#include <QSettings>
#include "libboardgame_util/Log.h"

using namespace std;
using libboardgame_util::log;
using libpentobi_base::to_string_id;
using libpentobi_base::BoardType;
using libpentobi_base::Color;
using libpentobi_base::ColorIterator;
using libpentobi_base::CoordPoint;
using libpentobi_base::MovePoints;
using libpentobi_base::PiecePoints;
using libpentobi_base::Point;

//-----------------------------------------------------------------------------

BoardModel::BoardModel(QObject* parent)
    : QObject(parent),
      m_bd(getInitialGameVariant()),
      m_gameVariant(to_string_id(m_bd.get_variant())),
      m_nuColors(m_bd.get_nu_colors()),
      m_nuPlayers(m_bd.get_nu_players()),
      m_toPlay(0),
      m_points0(0),
      m_points1(0),
      m_points2(0),
      m_points3(0),
      m_hasMoves0(true),
      m_hasMoves1(true),
      m_hasMoves2(true),
      m_hasMoves3(true),
      m_isGameOver(false),
      m_canUndo(false)
{
    createPieceModels();
    updateProperties();
}

void BoardModel::createPieceModels()
{
    m_pieceModels0.clear();
    m_pieceModels1.clear();
    m_pieceModels2.clear();
    m_pieceModels3.clear();
    createPieceModels(Color(0), m_pieceModels0);
    createPieceModels(Color(1), m_pieceModels1);
    if (m_nuColors > 2)
        createPieceModels(Color(2), m_pieceModels2);
    if (m_nuColors > 3)
        createPieceModels(Color(3), m_pieceModels3);
}

void BoardModel::createPieceModels(Color c, QList<PieceModel*>& pieceModels)
{
    pieceModels.clear();
    for (unsigned i = 0; i < m_bd.get_nu_pieces(); ++i)
        pieceModels.append(new PieceModel(this, m_bd,
                                          libpentobi_base::Piece(i), c));
}

bool BoardModel::findMove(const PieceModel& piece, QPointF coord,
                          Move& mv) const
{
    auto& info = m_bd.get_piece_info(piece.getPiece());
    PiecePoints piecePoints = info.get_points();
    piece.getTransform()->transform(piecePoints.begin(), piecePoints.end());
    QPointF center(PieceModel::findCenter(m_bd, piecePoints));
    auto& geo = m_bd.get_geometry();
    MovePoints points;
    for (auto& p : piecePoints)
    {
        qreal x = round(p.x - center.x() + coord.x());
        qreal y = round(p.y - center.y() + coord.y());
        if (! geo.is_onboard(CoordPoint(static_cast<int>(x),
                                        static_cast<int>(y))))
            return false;
        points.push_back(Point(static_cast<unsigned>(x),
                               static_cast<unsigned>(y),
                               geo.get_width()));
    }
    return m_bd.find_move(points, mv);
}

Variant BoardModel::getInitialGameVariant()
{
    QSettings settings;
    auto variantString = settings.value("variant", "").toString();
    Variant gameVariant;
    if (! parse_variant_id(variantString.toLocal8Bit().constData(),
                           gameVariant))
        // Use Duo on the first invocation because it is a better
        // default for mobile devices than Classic.
        gameVariant = Variant::duo;
    return gameVariant;
}

void BoardModel::initGameVariant(QString gameVariant)
{
    if (m_gameVariant == gameVariant)
        return;
    if (gameVariant == "classic")
        m_bd.init(Variant::classic);
    else if (gameVariant == "classic_2")
        m_bd.init(Variant::classic_2);
    else if (gameVariant == "duo")
        m_bd.init(Variant::duo);
    else if (gameVariant == "trigon")
        m_bd.init(Variant::trigon);
    else if (gameVariant == "trigon_2")
        m_bd.init(Variant::trigon_2);
    else
    {
        qDebug() << "BoardModel: invalid/unsupported game variant";
        return;
    }
    int nuColors = m_bd.get_nu_colors();
    if (nuColors != m_nuColors)
    {
        m_nuColors = nuColors;
        emit nuColorsChanged(nuColors);
    }
    int nuPlayers = m_bd.get_nu_players();
    if (nuPlayers != m_nuPlayers)
    {
        m_nuPlayers = nuPlayers;
        emit nuPlayersChanged(nuPlayers);
    }
    createPieceModels();
    m_gameVariant = gameVariant;
    emit gameVariantChanged(gameVariant);
    updateProperties();
    QSettings settings;
    settings.setValue("variant", gameVariant);
}

bool BoardModel::isLegalPos(PieceModel* pieceModel, QPointF coord) const
{
    Move mv;
    if (! findMove(*pieceModel, coord, mv))
        return false;
    Color c(pieceModel->color());
    bool result = m_bd.is_legal(c, mv);
    return result;
}

void BoardModel::newGame()
{
    m_bd.init();
    updateProperties();
}

QList<PieceModel*>& BoardModel::pieceModels(Color c)
{
    if (c == Color(0))
        return m_pieceModels0;
    else if (c == Color(1))
        return m_pieceModels1;
    else if (c == Color(2))
        return m_pieceModels2;
    else
        return  m_pieceModels3;
}

QQmlListProperty<PieceModel> BoardModel::pieceModels0()
{
    return QQmlListProperty<PieceModel>(this, m_pieceModels0);
}

QQmlListProperty<PieceModel> BoardModel::pieceModels1()
{
    return QQmlListProperty<PieceModel>(this, m_pieceModels1);
}

QQmlListProperty<PieceModel> BoardModel::pieceModels2()
{
    return QQmlListProperty<PieceModel>(this, m_pieceModels2);
}

QQmlListProperty<PieceModel> BoardModel::pieceModels3()
{
    return QQmlListProperty<PieceModel>(this, m_pieceModels3);
}

void BoardModel::play(PieceModel* pieceModel, QPointF coord)
{
    Color c(pieceModel->color());
    Move mv;
    if (! findMove(*pieceModel, coord, mv))
    {
        qDebug() << "BoardModel::play: illegal move";
        return;
    }
    play(c, mv);
}

void BoardModel::play(Color c, Move mv)
{
    m_bd.play(c, mv);
    updateProperties();
}

void BoardModel::updatePiece(Color c, Move mv)
{
    auto& info = m_bd.get_move_info(mv);
    auto& geo = m_bd.get_geometry();
    auto width = geo.get_width();
    for (auto pieceModel : pieceModels(c))
        if (pieceModel->getPiece() == info.get_piece())
        {
            PiecePoints movePoints;
            for (auto& p : info)
                movePoints.push_back(CoordPoint(p.get_x(width),
                                                p.get_y(width)));
            auto& pieceInfo = m_bd.get_piece_info(info.get_piece());
            auto transform = pieceInfo.find_transform(geo, movePoints);
            auto oldTransform = pieceModel->getTransform();
            if (transform != pieceInfo.get_equivalent_transform(oldTransform))
                pieceModel->setTransform(transform);
            QPointF center = PieceModel::findCenter(m_bd, movePoints);
            pieceModel->setGameCoord(center);
            pieceModel->setIsPlayed(true);
            return;
        }
}

void BoardModel::undo()
{
    if (m_bd.get_nu_moves() == 0)
        return;
    m_bd.undo();
    updateProperties();
}

void BoardModel::updateProperties()
{
    int points0 = m_bd.get_points(Color(0));
    if (m_points0 != points0)
    {
        m_points0 = points0;
        emit points0Changed(points0);
    }

    int points1 = m_bd.get_points(Color(1));
    if (m_points1 != points1)
    {
        m_points1 = points1;
        emit points1Changed(points1);
    }

    bool hasMoves0 = m_bd.has_moves(Color(0));
    if (m_hasMoves0 != hasMoves0)
    {
        m_hasMoves0 = hasMoves0;
        emit hasMoves0Changed(hasMoves0);
    }

    bool hasMoves1 = m_bd.has_moves(Color(1));
    if (m_hasMoves1 != hasMoves1)
    {
        m_hasMoves1 = hasMoves1;
        emit hasMoves1Changed(hasMoves1);
    }

    if (m_nuColors > 2)
    {
        int points2 = m_bd.get_points(Color(2));
        if (m_points2 != points2)
        {
            m_points2 = points2;
            emit points2Changed(points2);
        }

        bool hasMoves2 = m_bd.has_moves(Color(2));
        if (m_hasMoves2 != hasMoves2)
        {
            m_hasMoves2 = hasMoves2;
            emit hasMoves2Changed(hasMoves2);
        }
    }

    if (m_nuColors > 3)
    {
        int points3 = m_bd.get_points(Color(3));
        if (m_points3 != points3)
        {
            m_points3 = points3;
            emit points3Changed(points3);
        }

        bool hasMoves3 = m_bd.has_moves(Color(3));
        if (m_hasMoves3 != hasMoves3)
        {
            m_hasMoves3 = hasMoves3;
            emit hasMoves3Changed(hasMoves3);
        }
    }

    bool canUndo = (m_bd.get_nu_moves() > 0);
    if (m_canUndo != canUndo)
    {
        m_canUndo = canUndo;
        emit canUndoChanged(canUndo);
    }

    bool isGameOver = true;
    for (ColorIterator i(m_nuColors); i; ++i)
        if (m_bd.has_moves(*i))
            isGameOver = false;
    if (m_isGameOver != isGameOver)
    {
        m_isGameOver = isGameOver;
        emit isGameOverChanged(isGameOver);
    }

    for (ColorIterator i(m_nuColors); i; ++i)
    {
        // Does not handle game variant Junior yet (multiple instances of a
        // piece)
        LIBBOARDGAME_ASSERT(m_bd.get_nu_piece_instances() == 1);
        for (auto p : pieceModels(*i))
            if (m_bd.is_piece_left(*i, p->getPiece()))
                p->setIsPlayed(false);
    }
    // Does not handle setup yet
    for (ColorIterator i(m_nuColors); i; ++i)
        LIBBOARDGAME_ASSERT(m_bd.get_setup().placements[*i].empty());
    for (unsigned i = 0; i < m_bd.get_nu_moves(); ++i)
    {
        auto mv = m_bd.get_move(i);
        if (! mv.is_pass())
            updatePiece(mv.color, mv.move);
    }
    int toPlay = m_bd.get_effective_to_play().to_int();
    if (m_toPlay != toPlay)
    {
        m_toPlay = toPlay;
        emit toPlayChanged(toPlay);
    }
}

//-----------------------------------------------------------------------------
