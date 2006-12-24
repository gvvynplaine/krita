/* This file is part of the KDE project
 * Copyright (C) 2006 Jan Hambrecht <jaham@gmx.net>
 * Copyright (C) 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KOPATHCOMMAND_H
#define KOPATHCOMMAND_H

#include <QUndoCommand>
#include <QList>
#include <QMap>
#include <QPointF>
#include "KoPathShape.h"
#include <koffice_export.h>

class KoParameterShape;
class KoShapeAddRemoveData;

/// the base command for commands altering a path shape
class KoPathBaseCommand : public QUndoCommand {
public:
    /**
     * @param parent the parent command used for macro commands
     */
    KoPathBaseCommand( QUndoCommand *parent = 0 );
    /** initialize the base command with a single shape
     * @param parent the parent command used for macro commands
     */
    explicit KoPathBaseCommand( KoPathShape *shape, QUndoCommand *parent = 0 );
protected:
    /**
     * Shedules repainting of all shapes control point rects.
     * @param normalizeShape controls if paths are normalized before painting
     */
    void repaint( bool normalizeShapes );

    QSet<KoPathShape*> m_shapes; ///< the shapes the command operates on
};

/// The undo / redo command for path point moving.
class KoPointMoveCommand : public QUndoCommand 
{
public:
    /**
     * Command to move path point.
     * @param pointMap map of the path point to move
     * @param offset the offset by which the point is moved in document coordinates
     * @param parent the parent command used for macro commands
     */
    KoPointMoveCommand( const KoPathShapePointMap &pointMap, const QPointF &offset, QUndoCommand *parent = 0 );

    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    KoPathShapePointMap m_pointMap;
    QPointF m_offset;
};

/// The undo / redo command for path point moving.
class KoControlPointMoveCommand : public QUndoCommand
{
public:
    /**
     * Command to move one control path point.
     * @param point the path point to control point belongs to
     * @param offset the offset by which the point is moved in document coordinates
     * @param pointType the type of the point to move
     * @param parent the parent command used for macro commands
     */
    KoControlPointMoveCommand( KoPathPoint *point, const QPointF &offset, KoPathPoint::KoPointType pointType,
                               QUndoCommand *parent = 0 );
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    KoPathPoint * m_point;
    // the offset in shape coordinates
    QPointF m_offset;
    KoPathPoint::KoPointType m_pointType;
};

/// The undo / redo command for changing the path point type.
class KoPointPropertyCommand : public KoPathBaseCommand {
public:
    /**
     * Command to change the properties of a single point
     * @param point the path point to change properties for
     * @param property the new point properties to set
     * @param parent the parent command used for macro commands
     */
    KoPointPropertyCommand( KoPathPoint *point, KoPathPoint::KoPointProperties property, QUndoCommand *parent = 0 );
    /**
     * Command to change the properties of multiple points
     * @param points the path point whose properties to change
     * @param properties the new properties to set
     * @param parent the parent command used for macro commands
     */
    KoPointPropertyCommand( const QList<KoPathPoint*> &points, const QList<KoPathPoint::KoPointProperties> &properties,
                            QUndoCommand *parent = 0 );
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    typedef struct PointPropertyChangeset
    {
        KoPathPoint *point;
        KoPathPoint::KoPointProperties oldProperty;
        KoPathPoint::KoPointProperties newProperty;
        QPointF firstControlPoint;
        QPointF secondControlPoint;
    };
    QList<PointPropertyChangeset> m_changesets;
};

/// The undo / redo command for removing path points.
class KoPointRemoveCommand : public QUndoCommand {
public:
    /**
     * @brief Command to remove a points from path shapes
     * @param pointMap map of the path points to remove
     * @param parent the parent command used for macro commands
     */
    explicit KoPointRemoveCommand( const KoPathShapePointMap &pointMap, QUndoCommand *parent = 0 );
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    struct KoPointRemoveData
    {
        KoPointRemoveData( KoPathPoint * point, KoSubpath * subpath, int position )
        : m_point( point )
        , m_subpath( subpath )
        , m_position( position )
        {}
        KoPathPoint * m_point;
        KoSubpath * m_subpath;///< the position in the path 
        int m_position;
    };
    KoPathShapePointMap m_pointMap;
    QList<KoPointRemoveData> m_data;
};

/// The undo / redo command for splitting a path segment
class KoSegmentSplitCommand : public KoPathBaseCommand
{
public:
    /**
     * Command to split a single path segment at the given position
     * @param shape the path shape containing the points
     * @param segment the segment to split
     * @param splitPosition the position to split at [0..1]
     * @param parent the parent command used for macro commands
     */
    KoSegmentSplitCommand( KoPathShape *shape, const KoPathSegment &segment, double splitPosition,
                           QUndoCommand *parent = 0 );
    /**
     * Command to split multiple path segments at different positions
     * @param shape the path shape containing the points
     * @param segments the segments to split
     * @param splitPositions the positions to split at [0..1]
     * @param parent the parent command used for macro commands
     */
    KoSegmentSplitCommand( KoPathShape *shape, const QList<KoPathSegment> &segments, const QList<double> &splitPositions,
                           QUndoCommand *parent = 0 );
    /**
     * Command to split multiple path segments at the same position
     * @param shape the path shape containing the points
     * @param segments the segments to split
     * @param splitPosition the position to split at [0..1]
     * @param parent the parent command used for macro commands
     */
    KoSegmentSplitCommand( KoPathShape *shape, const QList<KoPathSegment> &segments, double splitPosition,
                           QUndoCommand *parent = 0 );
    virtual ~KoSegmentSplitCommand();
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    QList<KoPathSegment> m_segments;
    typedef QPair<KoPathPoint,KoPathPoint> KoSegmentData;
    QList<KoSegmentData> m_oldNeighbors;
    QList<KoSegmentData> m_newNeighbors;
    QList<double> m_splitPos;
    QList<KoPathPoint*> m_splitPoints;
    bool m_deletePoint;
    QList< QPair<KoSubpath*,int> > m_splitPointPos;
};

/// The undo / redo command for joining two start/end path points
class KoPointJoinCommand : public KoPathBaseCommand
{
public:
    /**
     * Command to join two start/end path points.
     * @param shape the path shape whose points to join
     * @param point1 the first point of the subpath to join
     * @param point2 the second point of the subpath to join
     * @param parent the parent command used for macro commands
     */
    KoPointJoinCommand( KoPathShape *shape, KoPathPoint *point1, KoPathPoint *point2, QUndoCommand *parent = 0 );
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
    /// return the name of this command
    QString name() const;
private:
    KoPathPoint* m_point1;
    KoPathPoint* m_point2;
    bool m_joined;
};

/// The undo / redo command for breaking a subpath
class KoSubpathBreakCommand : public KoPathBaseCommand
{
public:
    /**
     * Command to break a subpath at a single point.
     * @param shape the path shape whose subpath to close
     * @param breakPoint the point to break at
     * @param parent the parent command used for macro commands
     */
    KoSubpathBreakCommand( KoPathShape *shape, KoPathPoint *breakPoint, QUndoCommand *parent = 0 );
    /**
     * Command to break a subpath at a path segment
     * @param shape the path shape whose subpath to close
     * @param segment the segment
     * @param parent the parent command used for macro commands
     */
    KoSubpathBreakCommand( KoPathShape *shape, const KoPathSegment &segment, QUndoCommand *parent = 0 );
    virtual ~KoSubpathBreakCommand();
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    bool m_broken;
    KoPathSegment m_breakSegment;
    KoPathPoint* m_breakPoint;
    KoPathPoint* m_newPoint;
    typedef QPair<KoPathPoint*, KoPathPoint> PointData;
    QList<PointData> m_pointData;
};

/// The undo / redo command for changing a segments type (curve/line)
class KoSegmentTypeCommand : public KoPathBaseCommand
{
public:
    /**
     * Command for changing a segments type (curve/line)
     * @param shape the path shape whose subpath to close
     * @param segment the segment to change the type of
     * @param changeToLine if true, changes segment to line, else changes segment to curve
     * @param parent the parent command used for macro commands
     */
    KoSegmentTypeCommand( KoPathShape *shape, const KoPathSegment &segment, bool changeToLine, QUndoCommand *parent = 0 );
    /**
     * Command for changing a segments type (curve/line)
     * @param shape the path shape whose subpath to close
     * @param segments the segments to change the type of
     * @param changeToLine if true, changes segments to lines, else changes segments to curves
     * @param parent the parent command used for macro commands
     */
    KoSegmentTypeCommand( KoPathShape *shape, const QList<KoPathSegment> &segments, bool changeToLine,
                          QUndoCommand *parent = 0 );
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    QList<KoPathSegment> m_segments;
    QMap<KoPathPoint*,KoPathPoint> m_oldPointData;
    bool m_changeToLine;
};

class KoShapeControllerBase;

/// The undo / redo command for combining two or more paths into one
class FLAKE_EXPORT KoPathCombineCommand : public QUndoCommand
{
public:
    /**
     * Command for combining a list of paths into one single path.
     * @param controller the controller to used for removing/inserting.
     * @param paths the list of paths to combine
     * @param addRemoveData additional data for shape creation
     * @param parent the parent command used for macro commands
     */
    KoPathCombineCommand( KoShapeControllerBase *controller, const QList<KoPathShape*> &paths, 
                          KoShapeAddRemoveData *addRemoveData = 0, QUndoCommand *parent = 0 );
    virtual ~KoPathCombineCommand();
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    KoShapeControllerBase *m_controller;
    QList<KoPathShape*> m_paths;
    KoPathShape *m_combinedPath;
    bool m_isCombined;
    KoShapeAddRemoveData *m_addRemoveData;
};

/// The undo / redo command for changing a parameter
class KoParameterChangeCommand : public QUndoCommand
{
public:
    KoParameterChangeCommand( KoParameterShape *shape, int handleId, const QPointF &startPoint, const QPointF &endPoint,
                              QUndoCommand *parent = 0 );
    virtual ~KoParameterChangeCommand();

    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    KoParameterShape *m_shape;
    int m_handleId;
    QPointF m_startPoint;
    QPointF m_endPoint;
};

/// The undo / redo command for changing a KoParameterShape into a KoPathShape
class KoParameterToPathCommand : public QUndoCommand
{
public:
    KoParameterToPathCommand( KoParameterShape *shape, QUndoCommand *parent = 0 );
    KoParameterToPathCommand( const QList<KoParameterShape*> &shapes, QUndoCommand *parent = 0 );
    virtual ~KoParameterToPathCommand();

    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    QList<KoParameterShape*> m_shapes;
};

/// The undo / redo command for separating subpaths into different paths
class FLAKE_EXPORT KoPathSeparateCommand : public QUndoCommand
{
public:
    /**
     * Command for separating subpaths of a list of paths into different paths.
     * @param controller the controller to used for removing/inserting.
     * @param paths the list of paths to separate
     * @param addRemoveData additional data for shape creation
     * @param parent the parent command used for macro commands
     */
    KoPathSeparateCommand( KoShapeControllerBase *controller, const QList<KoPathShape*> &paths, 
                           KoShapeAddRemoveData *addRemoveData = 0, QUndoCommand *parent = 0 );
    virtual ~KoPathSeparateCommand();
    /// redo the command
    void redo();
    /// revert the actions done in redo
    void undo();
private:
    KoShapeControllerBase *m_controller;
    QList<KoPathShape*> m_paths;
    QList<KoPathShape*> m_separatedPaths;
    bool m_isSeparated;
    KoShapeAddRemoveData *m_addRemoveData;
};

#endif
