/* Tâche principale de la souris Conchodytes — voir mouse_task.c. */
#pragma once
#include "esp_err.h"

/* Initialise les entrées et le capteur, puis démarre la tâche de scrutin.
 * Rend ESP_OK même si le capteur manque : les clics et la molette restent
 * lisibles, et l'échec est journalisé. Ne rend une erreur que si la tâche
 * elle-même n'a pas pu démarrer. */
esp_err_t mouse_task_start(void);
