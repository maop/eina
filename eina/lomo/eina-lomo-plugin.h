/*
 * eina/lomo/eina-lomo-plugin.h
 *
 * Copyright (C) 2004-2011 Eina
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __EINA_LOMO_PLUGIN_H__
#define __EINA_LOMO_PLUGIN_H__

#include <lomo/lomo-player.h>
#include <lomo/lomo-util.h>
#include <eina/ext/eina-application.h>

G_BEGIN_DECLS

/**
 * SECTION: eina-lomo-plugin 
 * @short_description: The lomo plugin
 *
 * #EinaLomoPlugin provides the interface to liblomo in Eina
 *
 * See also: #LomoPlayer, #LomoStream
 */

/**
 * EinaLomoPluginError:
 * @EINA_LOMO_PLUGIN_ERROR_CANNOT_CREATE_ENGINE: LomoPlayer engine cannot be created
 * @EINA_LOMO_PLUGIN_ERROR_CANNOT_SET_SHARED: LomoPlayer engine cannot be inserted
 *                                            into EinaApplication
 * @EINA_LOMO_PLUGIN_ERROR_CANNOT_DESTROY_ENGINE: LomoPlayer engine cannot be
 *                                                destroyed
 *
 * Describes error generated by EinaLomoPlugin
 */
typedef enum {
	EINA_LOMO_PLUGIN_ERROR_CANNOT_CREATE_ENGINE = 1,
	EINA_LOMO_PLUGIN_ERROR_CANNOT_SET_SHARED,
	EINA_LOMO_PLUGIN_ERROR_CANNOT_DESTROY_ENGINE
} EinaLomoPluginError;

/*
 * EinaApplication accessors
 */
LomoPlayer* eina_application_get_lomo(EinaApplication *application);

/**
 * EINA_LOMO_PREFERENCES_DOMAIN:
 *
 * Domain for EinaLomoPlugin preferences
 */
#define EINA_LOMO_PREFERENCES_DOMAIN  EINA_DOMAIN".preferences.lomo"

/**
 * EINA_LOMO_VOLUME_KEY:
 *
 * Preferences key for the volume setting
 */
#define EINA_LOMO_VOLUME_KEY          "volume"

/**
 * EINA_LOMO_MUTE_KEY:
 *
 * Preferences key for the mute setting
 */
#define EINA_LOMO_MUTE_KEY            "mute"

/**
 * EINA_LOMO_REPEAT_KEY:
 *
 * Preferences key for the repeat setting
 */
#define EINA_LOMO_REPEAT_KEY          "repeat"

/**
 * EINA_LOMO_RANDOM_KEY:
 *
 * Preferences key for the random setting
 */
#define EINA_LOMO_RANDOM_KEY          "random"

/**
 * EINA_LOMO_AUTO_PARSE_KEY:
 *
 * Preferences key for the auto-parse setting
 */
#define EINA_LOMO_AUTO_PARSE_KEY      "auto-parse"

/**
 * EINA_LOMO_AUTO_PLAY_KEY:
 *
 * Preferences key for the auto-play setting
 */
#define EINA_LOMO_AUTO_PLAY_KEY       "auto-play"

/**
 * EINA_LOMO_GAPLESS_MODE_KEY
 *
 * Preferences key for the gapless-mode setting
 */
#define EINA_LOMO_GAPLESS_MODE_KEY    "gapless-mode"

G_END_DECLS

#endif /* __EINA_LOMO_PLUGIN_H__ */
