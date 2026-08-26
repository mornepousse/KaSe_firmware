/* Voir mouse_buttons.h pour le raisonnement. Aucune dépendance ESP-IDF ici :
 * ce fichier est compilé tel quel par le harnais de test hôte. */
#include "mouse_buttons.h"

mouse_contact_t mouse_contact_decode(int no_level, int nc_level)
{
    if (no_level && !nc_level) return MOUSE_CONTACT_RELEASED;
    if (!no_level && nc_level) return MOUSE_CONTACT_PRESSED;
    if (no_level && nc_level)  return MOUSE_CONTACT_BOUNCING;
    return MOUSE_CONTACT_IMPOSSIBLE;
}

bool mouse_button_next(bool prev, mouse_contact_t contact)
{
    switch (contact) {
    case MOUSE_CONTACT_PRESSED:  return true;
    case MOUSE_CONTACT_RELEASED: return false;
    /* Les deux cas ambigus retiennent l'état précédent. Les garder distincts
     * plutôt que de les fondre en `default` a un intérêt : IMPOSSIBLE signale
     * un défaut matériel et mérite d'être compté à part par l'appelant, même
     * si la décision est la même. */
    case MOUSE_CONTACT_BOUNCING:
    case MOUSE_CONTACT_IMPOSSIBLE:
    default:
        return prev;
    }
}
