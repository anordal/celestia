// configfile.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <iostream>
#include <fstream>
#include <cassert>
#include <celutil/debug.h>
#include <celutil/directory.h>
#include <celutil/util.h>
#include <celengine/celestia.h>
#include <celengine/parser.h>
#include "configfile.h"

using namespace std;


static unsigned int getUint(Hash* params,
                            const string& paramName,
                            unsigned int defaultValue)
{
    double value = 0.0;
    if (params->getNumber(paramName, value))
        return (unsigned int) value;
    else
        return defaultValue;
}


CelestiaConfig* ReadCelestiaConfig(string filename, CelestiaConfig *config)
{
    ifstream configFile(filename.c_str());
    if (!configFile.good())
    {
        DPRINTF(0, "Error opening config file.");
        return config;
    }

    Tokenizer tokenizer(&configFile);
    Parser parser(&tokenizer);

    if (tokenizer.nextToken() != Tokenizer::TokenName)
    {
        DPRINTF(0, "%s:%d 'Configuration' expected.\n", filename.c_str(),
                tokenizer.getLineNumber());
        return config;
    }

    if (tokenizer.getStringValue() != "Configuration")
    {
        DPRINTF(0, "%s:%d 'Configuration' expected.\n", filename.c_str(),
                tokenizer.getLineNumber());
        return config;
    }

    Value* configParamsValue = parser.readValue();
    if (configParamsValue == NULL || configParamsValue->getType() != Value::HashType)
    {
        DPRINTF(0, "%s: Bad configuration file.\n", filename.c_str());
        return config;
    }

    Hash* configParams = configParamsValue->getHash();

    if (config == NULL) config = new CelestiaConfig();

    config->faintestVisible = 6.0f;
    configParams->getNumber("FaintestVisibleMagnitude", config->faintestVisible);
    configParams->getString("FavoritesFile", config->favoritesFile);
    config->favoritesFile = WordExp(config->favoritesFile);
    configParams->getString("DestinationFile", config->destinationsFile);
    config->destinationsFile = WordExp(config->destinationsFile);
    configParams->getString("InitScript", config->initScriptFile);
    config->initScriptFile = WordExp(config->initScriptFile);
    configParams->getString("DemoScript", config->demoScriptFile);
    config->demoScriptFile = WordExp(config->demoScriptFile);
    configParams->getString("AsterismsFile", config->asterismsFile);
    config->asterismsFile = WordExp(config->asterismsFile);
    configParams->getString("BoundariesFile", config->boundariesFile);
    config->boundariesFile = WordExp(config->boundariesFile);
    configParams->getString("DeepSkyCatalog", config->deepSkyCatalog);
    config->deepSkyCatalog = WordExp(config->deepSkyCatalog);
    configParams->getString("StarDatabase", config->starDatabaseFile);
    config->starDatabaseFile = WordExp(config->starDatabaseFile);
    configParams->getString("StarNameDatabase", config->starNamesFile);
    config->starNamesFile = WordExp(config->starNamesFile);
    configParams->getString("HDCrossIndex", config->HDCrossIndexFile);
    config->HDCrossIndexFile = WordExp(config->HDCrossIndexFile);
    configParams->getString("SAOCrossIndex", config->SAOCrossIndexFile);
    config->SAOCrossIndexFile = WordExp(config->SAOCrossIndexFile);
    configParams->getString("GlieseCrossIndex", config->GlieseCrossIndexFile);
    config->GlieseCrossIndexFile = WordExp(config->GlieseCrossIndexFile);
    configParams->getString("Font", config->mainFont);
    configParams->getString("LabelFont", config->labelFont);
    configParams->getString("TitleFont", config->titleFont);
    configParams->getString("LogoTexture", config->logoTextureFile);
    configParams->getString("Cursor", config->cursor);
    // configParams->getNumber("LogoWidth", config->logoWidth);
    // configParams->getNumber("LogoHeight", config->logoHeight);

    double aaSamples = 1;
    configParams->getNumber("AntialiasingSamples", aaSamples);
    config->aaSamples = (unsigned int) aaSamples;

    config->hdr = false;
    configParams->getBoolean("HighDynamicRange", config->hdr);

    config->rotateAcceleration = 120.0f;
    configParams->getNumber("RotateAcceleration", config->rotateAcceleration);
    config->mouseRotationSensitivity = 1.0f;
    configParams->getNumber("MouseRotationSensitivity", config->mouseRotationSensitivity);
    configParams->getBoolean("ReverseMouseWheel", config->reverseMouseWheel);
    configParams->getString("ScriptScreenshotDirectory", config->scriptScreenshotDirectory);
    config->scriptScreenshotDirectory = WordExp(config->scriptScreenshotDirectory);
    config->scriptSystemAccessPolicy = "ask";
    configParams->getString("ScriptSystemAccessPolicy", config->scriptSystemAccessPolicy);

    config->ringSystemSections = getUint(configParams, "RingSystemSections", 100);
    config->orbitPathSamplePoints = getUint(configParams, "OrbitPathSamplePoints", 100);
    config->shadowTextureSize = getUint(configParams, "ShadowTextureSize", 256);
    config->eclipseTextureSize = getUint(configParams, "EclipseTextureSize", 128);

    Value* solarSystemsVal = configParams->getValue("SolarSystemCatalogs");
    if (solarSystemsVal != NULL)
    {
        if (solarSystemsVal->getType() != Value::ArrayType)
        {
            DPRINTF(0, "%s: SolarSystemCatalogs must be an array.\n", filename.c_str());
        }
        else
        {
            Array* solarSystems = solarSystemsVal->getArray();
            // assert(solarSystems != NULL);

            for (Array::iterator iter = solarSystems->begin(); iter != solarSystems->end(); iter++)
            {
                Value* catalogNameVal = *iter;
                // assert(catalogNameVal != NULL);
                if (catalogNameVal->getType() == Value::StringType)
                {
                    config->solarSystemFiles.insert(config->solarSystemFiles.end(),
                                                    WordExp(catalogNameVal->getString()));
                }
                else
                {
                    DPRINTF(0, "%s: Solar system catalog name must be a string.\n",
                            filename.c_str());
                }
            }
        }
    }

    Value* starCatalogsVal = configParams->getValue("StarCatalogs");
    if (starCatalogsVal != NULL)
    {
        if (starCatalogsVal->getType() != Value::ArrayType)
        {
            DPRINTF(0, "%s: StarCatalogs must be an array.\n",
                    filename.c_str());
        }
        else
        {
            Array* starCatalogs = starCatalogsVal->getArray();
            assert(starCatalogs != NULL);

            for (Array::iterator iter = starCatalogs->begin();
                 iter != starCatalogs->end(); iter++)
            {
                Value* catalogNameVal = *iter;
                assert(catalogNameVal != NULL);

                if (catalogNameVal->getType() == Value::StringType)
                {
                    config->starCatalogFiles.push_back(WordExp(catalogNameVal->getString()));
                }
                else
                {
                    DPRINTF(0, "%s: Star catalog name must be a string.\n",
                            filename.c_str());
                }
            }
        }
    }

    Value* extrasDirsVal = configParams->getValue("ExtrasDirectories");
    if (extrasDirsVal != NULL)
    {
        if (extrasDirsVal->getType() == Value::ArrayType)
        {
            Array* extrasDirs = extrasDirsVal->getArray();
            assert(extrasDirs != NULL);

            for (Array::iterator iter = extrasDirs->begin();
                 iter != extrasDirs->end(); iter++)
            {
                Value* dirNameVal = *iter;
                if (dirNameVal->getType() == Value::StringType)
                {
                    config->extrasDirs.insert(config->extrasDirs.end(),
                                              WordExp(dirNameVal->getString()));
                }
                else
                {
                    DPRINTF(0, "%s: Extras directory name must be a string.\n",
                            filename.c_str());
                }
            }
        }
        else if (extrasDirsVal->getType() == Value::StringType)
        {
            config->extrasDirs.insert(config->extrasDirs.end(),
                                      WordExp(extrasDirsVal->getString()));
        }
        else
        {
            DPRINTF(0, "%s: ExtrasDirectories must be an array or string.\n", filename.c_str());
        }
    }

    Value* labelledStarsVal = configParams->getValue("LabelledStars");
    if (labelledStarsVal != NULL)
    {
        if (labelledStarsVal->getType() != Value::ArrayType)
        {
            DPRINTF(0, "%s: LabelledStars must be an array.\n", filename.c_str());
        }
        else
        {
            Array* labelledStars = labelledStarsVal->getArray();
            // assert(labelledStars != NULL);

            for (Array::iterator iter = labelledStars->begin(); iter != labelledStars->end(); iter++)
            {
                Value* starNameVal = *iter;
                // assert(starNameVal != NULL);
                if (starNameVal->getType() == Value::StringType)
                {
                    config->labelledStars.insert(config->labelledStars.end(),
                                                 starNameVal->getString());
                }
                else
                {
                    DPRINTF(0, "%s: Star name must be a string.\n", filename.c_str());
                }
            }
        }
    }

    Value* ignoreExtVal = configParams->getValue("IgnoreGLExtensions");
    if (ignoreExtVal != NULL)
    {
        if (ignoreExtVal->getType() != Value::ArrayType)
        {
            DPRINTF(0, "%s: IgnoreGLExtensions must be an array.\n",
                    filename.c_str());
        }
        else
        {
            Array* ignoreExt = ignoreExtVal->getArray();

            for (Array::iterator iter = ignoreExt->begin();
                 iter != ignoreExt->end(); iter++)
            {
                Value* extVal = *iter;
                if (extVal->getType() == Value::StringType)
                    config->ignoreGLExtensions.push_back(extVal->getString());
                else
                    DPRINTF(0, "%s: extension name must be a string.\n", filename.c_str());
            }
        }
    }

    delete configParamsValue;

    return config;
}
