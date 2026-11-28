#ifndef EIDOS_PLATFORM_PS4_LOG_H
#define EIDOS_PLATFORM_PS4_LOG_H

/* Run log for a console that has no terminal.
 *
 * Everything the program writes to stdout and stderr is sent to a file.  The
 * file is EIDOS_LOG.txt in the root of the first USB drive that accepts
 * writes (/mnt/usb0 ...), if the application can see one.  The SDK documents
 * no application access to USB storage and on a console running homebrew the
 * sandbox usually shows none, so the file is normally
 * /data/eidos/EIDOS_LOG.txt: read it over the console's file transfer
 * service (tools/ps4/fetch_log.sh).  eidos_ps4_log_export() copies it to a drive
 * as soon as one is there (at a setup error and on exit).  A log a run left on
 * internal storage is copied to the drive as EIDOS_LOG_PREV.txt at the next start.
 *
 * The Orbis SDK gives an application no signal or exception hook, so a
 * crash cannot be reported from inside.  Every orderly exit ends the log with
 * "==== clean exit ====": a log without that line means the program crashed or
 * hung, and its last line says where.  Nothing in the log comes from the
 * game's data. */

#ifdef __cplusplus
extern "C" {
#endif

/* Open the log.  Safe to call twice. */
void eidos_ps4_log_start(const char *title_id, const char *version);

/* Build-time provenance generated from the linked shader manifest. */
const char *eidos_ps4_build_provenance(void);

/* Copy the log to a USB drive when it is not already on one.  Returns 0 when
 * the log is on a drive afterwards, -1 when no drive accepted it. */
int eidos_ps4_log_export(void);

/* Write the closing line with the exit status and copy the log to a drive.
 * For an exit that does not go through exit() (sceSystemServiceLoadExec). */
void eidos_ps4_log_finish(int status);

/* Where the log is now (for a message to the player); "" before the start. */
const char *eidos_ps4_log_path(void);

#ifdef __cplusplus
}
#endif

#endif
