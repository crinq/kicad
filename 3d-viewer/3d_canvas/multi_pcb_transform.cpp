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

#include "multi_pcb_transform.h"

#include <board.h>
#include <board_item.h>
#include <zone.h>
#include <pcb_text.h>
#include <footprint.h>
#include <layer_ids.h>
#include <base_units.h>

#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/log.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>


bool MULTI_PCB_AREA::Contains( const VECTOR2I& aPoint ) const
{
    return outline.Contains( aPoint );
}


/**
 * Parse a single value with unit from the transform string.
 * Handles formats like "5mm", "-5mm", "10.5mm", "90°", "90deg"
 */
static bool parseValueWithUnit( const wxString& aToken, double& aValue, bool aIsAngle )
{
    wxString cleaned = aToken.Strip( wxString::both );

    if( cleaned.IsEmpty() )
        return false;

    // Remove unit suffix
    wxString numStr;

    if( aIsAngle )
    {
        // Remove ° or "deg" suffix
        if( cleaned.EndsWith( wxT( "\u00B0" ) ) )  // degree symbol
            numStr = cleaned.Left( cleaned.Length() - 1 );
        else if( cleaned.EndsWith( wxT( "°" ) ) )
            numStr = cleaned.Left( cleaned.Length() - wxString( wxT( "°" ) ).Length() );
        else if( cleaned.Lower().EndsWith( wxT( "deg" ) ) )
            numStr = cleaned.Left( cleaned.Length() - 3 );
        else
            numStr = cleaned;  // assume degrees with no suffix
    }
    else
    {
        // Remove "mm" or "in" suffix
        if( cleaned.Lower().EndsWith( wxT( "mm" ) ) )
            numStr = cleaned.Left( cleaned.Length() - 2 );
        else if( cleaned.Lower().EndsWith( wxT( "in" ) ) )
        {
            numStr = cleaned.Left( cleaned.Length() - 2 );
            double val;
            if( !numStr.ToDouble( &val ) )
                return false;
            aValue = val * 25.4;  // convert inches to mm
            return true;
        }
        else
            numStr = cleaned;  // assume mm with no suffix
    }

    return numStr.ToDouble( &aValue );
}


bool ParseMultiPcbTransform( const wxString& aText, MULTI_PCB_TRANSFORM_DATA& aResult )
{
    aResult = MULTI_PCB_TRANSFORM_DATA();

    wxString text = aText.Strip( wxString::both );

    // Remove "transform:" prefix if present
    if( text.Lower().StartsWith( wxT( "transform:" ) ) )
        text = text.Mid( 10 ).Strip( wxString::both );
    else if( text.Lower().StartsWith( wxT( "transform" ) ) )
        text = text.Mid( 9 ).Strip( wxString::both );

    // Parse key-value pairs: "x 5mm y -5mm z 10mm a 90° b 0° c 0°"
    wxStringTokenizer tokenizer( text, wxT( " \t" ), wxTOKEN_STRTOK );

    while( tokenizer.HasMoreTokens() )
    {
        wxString key = tokenizer.GetNextToken().Lower();

        if( !tokenizer.HasMoreTokens() )
            return false;

        wxString valueStr = tokenizer.GetNextToken();
        double   value = 0.0;

        bool isAngle = ( key == wxT( "a" ) || key == wxT( "b" ) || key == wxT( "c" ) );

        if( !parseValueWithUnit( valueStr, value, isAngle ) )
            return false;

        if( key == wxT( "x" ) )
            aResult.x = value;
        else if( key == wxT( "y" ) )
            aResult.y = value;
        else if( key == wxT( "z" ) )
            aResult.z = value;
        else if( key == wxT( "a" ) )
            aResult.a = value;
        else if( key == wxT( "b" ) )
            aResult.b = value;
        else if( key == wxT( "c" ) )
            aResult.c = value;
        else
            return false;  // Unknown key
    }

    return true;
}


std::vector<MULTI_PCB_AREA> ScanMultiPcbAreas( const BOARD* aBoard )
{
    std::vector<MULTI_PCB_AREA> areas;

    if( !aBoard )
        return areas;

    // Step 1: Find all zones on User.Comments layer whose name starts with "transform"
    for( ZONE* zone : aBoard->Zones() )
    {
        if( !zone->GetLayerSet().test( Cmts_User ) )
            continue;

        wxString zoneName = zone->GetZoneName().Lower();

        if( !zoneName.StartsWith( wxT( "transform" ) ) )
            continue;

        MULTI_PCB_AREA area;
        area.name = zone->GetZoneName();
        area.outline = *zone->Outline();

        // Compute center of the zone
        BOX2I bbox = zone->GetBoundingBox();
        area.center = bbox.Centre();

        // Build bbox caches for fast Contains() checks
        area.outline.BuildBBoxCaches();

        // Step 2: Find text items inside this zone on User.Comments layer
        // that contain the transform specification
        bool foundTransform = false;

        for( BOARD_ITEM* item : aBoard->Drawings() )
        {
            if( item->Type() != PCB_TEXT_T )
                continue;

            PCB_TEXT* text = static_cast<PCB_TEXT*>( item );

            if( text->GetLayer() != Cmts_User )
                continue;

            // Check if text center is inside the zone
            VECTOR2I textPos = text->GetPosition();

            if( !area.outline.Contains( textPos ) )
                continue;

            // Try to parse as transform
            wxString textContent = text->GetText();

            if( textContent.Lower().Contains( wxT( "transform" ) )
                || textContent.Lower().StartsWith( wxT( "x " ) ) )
            {
                if( ParseMultiPcbTransform( textContent, area.transform ) )
                {
                    foundTransform = true;
                    break;
                }
            }
        }

        // Also check text items inside footprints
        if( !foundTransform )
        {
            for( FOOTPRINT* fp : aBoard->Footprints() )
            {
                for( BOARD_ITEM* item : fp->GraphicalItems() )
                {
                    if( item->Type() != PCB_TEXT_T )
                        continue;

                    PCB_TEXT* text = static_cast<PCB_TEXT*>( item );

                    if( text->GetLayer() != Cmts_User )
                        continue;

                    VECTOR2I textPos = text->GetPosition();

                    if( !area.outline.Contains( textPos ) )
                        continue;

                    wxString textContent = text->GetText();

                    if( textContent.Lower().Contains( wxT( "transform" ) )
                        || textContent.Lower().StartsWith( wxT( "x " ) ) )
                    {
                        if( ParseMultiPcbTransform( textContent, area.transform ) )
                        {
                            foundTransform = true;
                            break;
                        }
                    }
                }

                if( foundTransform )
                    break;
            }
        }

        // Only add the area if it has a non-identity transform or if we found a transform string
        // (identity transforms with an explicit "transform" zone are kept to allow areas without
        //  transform for the "default" PCB position)
        areas.push_back( area );
    }

    return areas;
}


glm::mat4 BuildTransformMatrix( const MULTI_PCB_TRANSFORM_DATA& aTransform,
                                double aBiuTo3Dunits )
{
    if( aTransform.IsIdentity() )
        return glm::mat4( 1.0f );

    glm::mat4 mat( 1.0f );

    // Convert mm to 3D units
    // PCB_IU_PER_MM = 1e6 (1 IU = 1 nm, 1mm = 1e6 IU)
    float tx = (float)( aTransform.x * PCB_IU_PER_MM * aBiuTo3Dunits );
    float ty = (float)( -aTransform.y * PCB_IU_PER_MM * aBiuTo3Dunits );  // Y is inverted
    float tz = (float)( aTransform.z * PCB_IU_PER_MM * aBiuTo3Dunits );

    // Apply translation
    mat = glm::translate( mat, glm::vec3( tx, ty, tz ) );

    // Apply rotations: a around X, b around Y, c around Z
    if( aTransform.c != 0.0 )
        mat = glm::rotate( mat, glm::radians( (float)aTransform.c ), glm::vec3( 0.0f, 0.0f, 1.0f ) );

    if( aTransform.b != 0.0 )
        mat = glm::rotate( mat, glm::radians( (float)aTransform.b ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

    if( aTransform.a != 0.0 )
        mat = glm::rotate( mat, glm::radians( (float)aTransform.a ), glm::vec3( 1.0f, 0.0f, 0.0f ) );

    return mat;
}


glm::mat4 BuildTransformMatrix( const MULTI_PCB_TRANSFORM_DATA& aTransform,
                                double aBiuTo3Dunits,
                                float aFactor )
{
    if( aFactor <= 0.0f || aTransform.IsIdentity() )
        return glm::mat4( 1.0f );

    if( aFactor >= 1.0f )
        return BuildTransformMatrix( aTransform, aBiuTo3Dunits );

    MULTI_PCB_TRANSFORM_DATA interpolated;
    interpolated.x = aTransform.x * aFactor;
    interpolated.y = aTransform.y * aFactor;
    interpolated.z = aTransform.z * aFactor;
    interpolated.a = aTransform.a * aFactor;
    interpolated.b = aTransform.b * aFactor;
    interpolated.c = aTransform.c * aFactor;

    return BuildTransformMatrix( interpolated, aBiuTo3Dunits );
}


int FindMultiPcbArea( const std::vector<MULTI_PCB_AREA>& aAreas, const VECTOR2I& aPos )
{
    for( size_t i = 0; i < aAreas.size(); i++ )
    {
        if( aAreas[i].Contains( aPos ) )
            return (int)i;
    }

    return -1;
}
