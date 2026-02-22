/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef MULTI_PCB_TRANSFORM_H
#define MULTI_PCB_TRANSFORM_H

#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <wx/string.h>
#include <math/vector2d.h>
#include <geometry/shape_poly_set.h>

class BOARD;
class BOARD_ITEM;
class ZONE;


/**
 * Represents a 6-DOF transform for a sub-PCB in the multi-PCB assembly.
 *
 * Translation is in mm, rotation angles in degrees.
 * The convention is:
 *   x, y, z  - translation offset
 *   a        - rotation around Z axis (yaw)
 *   b        - rotation around Y axis (pitch)
 *   c        - rotation around X axis (roll)
 */
struct MULTI_PCB_TRANSFORM_DATA
{
    double x = 0.0;    ///< Translation X in mm
    double y = 0.0;    ///< Translation Y in mm
    double z = 0.0;    ///< Translation Z in mm
    double a = 0.0;    ///< Rotation around Z axis in degrees
    double b = 0.0;    ///< Rotation around Y axis in degrees
    double c = 0.0;    ///< Rotation around X axis in degrees

    bool IsIdentity() const
    {
        return x == 0.0 && y == 0.0 && z == 0.0
            && a == 0.0 && b == 0.0 && c == 0.0;
    }
};


/**
 * Represents a named area on the board that defines a sub-PCB with a transform.
 *
 * The area is defined by a zone on User.Comments layer with a name starting with
 * "transform". Inside the zone, a text field on User.Comments contains the transform
 * description, e.g.: "transform: x 5mm y -5mm z 10mm a 90° b 0° c 0°"
 */
struct MULTI_PCB_AREA
{
    wxString                 name;       ///< Zone name
    SHAPE_POLY_SET           outline;    ///< The zone outline polygon
    MULTI_PCB_TRANSFORM_DATA transform;  ///< The parsed transform
    VECTOR2I                 center;     ///< Center of the zone area (in BIU)

    /**
     * Check if a point (in BIU) is inside this area.
     */
    bool Contains( const VECTOR2I& aPoint ) const;
};


/**
 * Parse a transform string like "transform: x 5mm y -5mm z 10mm a 90° b 0° c 0°"
 *
 * @param aText the transform text to parse
 * @param aResult output transform data
 * @return true if parsing succeeded
 */
bool ParseMultiPcbTransform( const wxString& aText, MULTI_PCB_TRANSFORM_DATA& aResult );


/**
 * Scan the board for multi-PCB transform areas.
 *
 * Looks for rule areas / zones on User.Comments layer whose name starts with "transform",
 * then searches for text items inside those zones that contain the transform specification.
 *
 * @param aBoard the board to scan
 * @return list of found multi-PCB areas with their transforms
 */
std::vector<MULTI_PCB_AREA> ScanMultiPcbAreas( const BOARD* aBoard );


/**
 * Build a 4x4 transformation matrix from a MULTI_PCB_TRANSFORM_DATA.
 *
 * The matrix applies translation (x,y,z in 3D units) and rotation (a,b,c in degrees).
 * The biuTo3Dunits factor and board center offset are used to convert the mm-based
 * transform into the 3D viewer's coordinate system.
 *
 * @param aTransform the transform specification
 * @param aBiuTo3Dunits conversion factor from board internal units to 3D units
 * @return the 4x4 transformation matrix
 */
glm::mat4 BuildTransformMatrix( const MULTI_PCB_TRANSFORM_DATA& aTransform,
                                double aBiuTo3Dunits );


/**
 * Find which multi-PCB area a board position belongs to.
 *
 * @param aAreas list of multi-PCB areas
 * @param aPos position in BIU to test
 * @return index into aAreas, or -1 if not in any area
 */
int FindMultiPcbArea( const std::vector<MULTI_PCB_AREA>& aAreas, const VECTOR2I& aPos );


#endif // MULTI_PCB_TRANSFORM_H
