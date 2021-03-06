#ifndef _OL_PLAYER_RHYTHMBOX_H_
#define _OL_PLAYER_RHYTHMBOX_H_

#include "ol_player.h"

/** 
 * @brief Creates a controller of Rhythmbox
 * 
 * @return The controller of Rhythmbox. It's allocated by g_new, so use g_free to free the memory
 */
struct OlPlayer* ol_player_rhythmbox_get ();

#endif /* _OL_PLAYER_RHYTHMBOX_H_ */
