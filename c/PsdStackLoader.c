/*
** PsdStackloader loads the ENVARC:Sys/poseidon.prefs file and starts the stack.
** This is AROS specific, as under AmigaOS, the PsdStackloader is the config itself
** and under MorphOS, IPrefs loads the prefs.
*/

#include <proto/poseidon.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <poseidon_version.h>

const char *psd_version = PSD_VER("PsdStackLoader") ", by Chris Hodges <chrisly@platon42.de>";

int main(void)
{
    struct Library *ps;
    int ret = RETURN_FAIL;
    if((ps = OpenLibrary("poseidon.library", POSEIDON_LIB_MIN_VERSION)))
    {
        if(psdLoadCfgFromDisk(NULL))
        {
            ret = RETURN_OK;
            psdParseCfg();
        } else {
            ret = RETURN_ERROR;
            PutStr("Error loading poseidon.prefs!\n");
        }
        CloseLibrary(ps);
    } else {
        PutStr("Unable to open poseidon.library\n");
    }
    return(ret);
}
