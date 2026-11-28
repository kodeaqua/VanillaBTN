#ifndef MCARD_H
#define MCARD_H

/* Memory-card test screen: shows the card's files, formats and unformats it, and
 * renames the save files to their released names. */


void mcard_screen_tick(void);
#ifdef __psyz
void mcard_screen_enter(void);
#endif

#endif /* MCARD_H */
