
/*+
 * (C) JOANNEUM RESEARCH; http://www.joanneum.at
 * AUTHOR   : Christoph Eichkitz; http://www.joanneum.at/resources/gph/mitarbeiterinnen/mitarbeiter-detailansicht/person/0/3144/eichkitz.html
 * DATE     : November 2013
-*/

static const char* rcsID mUsedVar = "$Id$";

#include "uiGLCM_attrib.h"
#include "uiglcmmod.h"

#include "odplugin.h"
#include "uiodmain.h"
#include "uimain.h"

#include <iostream>

mDefODPluginInfo(uiGLCM)
{
    mDefineStaticLocalObject( PluginInfo, retpi,(
	"GLCM (Joanneum Research)",
	"Christoph Eichkitz",
	"1.06",
	"Plugin for the calculation of Grey level co-occurrence matrix-based attributes" ));
    return &retpi;
}


mDefODInitPlugin(uiGLCM)
{
    uiGLCM_attrib::initClass();
    return 0;
}
