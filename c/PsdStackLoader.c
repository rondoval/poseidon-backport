/*
** PsdStackLoader starts the stack: it is the one command the startup needs.
**
** What should run comes from ENVARC:Sys/poseidon.prefs. Where the prefs say
** nothing, defaults stand in: every class in SYS:Classes/USB, and the host
** controller named on the command line. AddUSBClasses and AddUSBHardware do
** those two jobs by hand; they are not needed at startup.
*/

#include <string.h>
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <libraries/poseidon.h>
#include <proto/poseidon.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <poseidon_version.h>
#include <classdir.h>

#define ARGS_DEVICE   0
#define ARGS_UNIT     1
#define ARGS_SIZEOF   2

static const char *template = "DEVICE,UNIT/N";
const char *psd_version = PSD_VER("PsdStackLoader") ", by Chris Hodges <chrisly@platon42.de>";

/* TRUE if no host controller is attached to the stack. */
static BOOL noHardware(struct Library *ps)
{
    struct List *phwlist;
    BOOL none;

    psdLockReadPBase();
    psdGetAttrs(PGA_STACK, NULL, PA_HardwareList, &phwlist, TAG_END);
    none = (phwlist->lh_Head->ln_Succ == NULL);
    psdUnlockPBase();

    return(none);
}

int main(void)
{
    IPTR ArgsArray[ARGS_SIZEOF] = { 0, 0 };
    struct RDArgs *ArgsHook;
    struct Library *ps;
    int ret = RETURN_OK;

    if(!(ArgsHook = ReadArgs(template, ArgsArray, NULL)))
    {
        PutStr("Wrong arguments!\n");
        return(RETURN_FAIL);
    }

    if((ps = OpenLibrary("poseidon.library", POSEIDON_LIB_MIN_VERSION)))
    {
        /* No prefs file is not an error: the defaults below cover it. */
        psdLoadCfgFromDisk(NULL);

        /* "The prefs exist" does not mean "the prefs say what to run". The
           controller and class lists are written by Trident only; prefs saved
           from a class settings window carry neither. So the defaults are
           keyed on what the config lists, not on whether a file was found. */
        APTR pic = psdFindCfgForm(NULL, IFFFORM_STACKCFG);

        if(!(pic && psdFindCfgForm(pic, IFFFORM_USBCLASS)))
        {
            if(!psdAddClassDir(ps, FALSE))
            {
                PutStr("Failed to lock " PSD_CLASSDRAWER ".\n");
                ret = RETURN_WARN;
            }
        }

        /* A stack started from ROM has its controller already; so has one whose
           prefs list it, once they are applied below. */
        if(ArgsArray[ARGS_DEVICE] && noHardware(ps) &&
           !(pic && psdFindCfgForm(pic, IFFFORM_UHWDEVICE)))
        {
            STRPTR devname = (STRPTR) ArgsArray[ARGS_DEVICE];
            ULONG unit = ArgsArray[ARGS_UNIT] ? *((ULONG *) ArgsArray[ARGS_UNIT]) : 0;
            APTR phw = psdAddHardware(devname, unit);

            if(phw && !psdEnumerateHardware(phw))
            {
                psdRemHardware(phw);
                phw = NULL;
            }
            if(!phw)
            {
                Printf("Unable to start %s unit %ld.\n", devname, unit);
                ret = RETURN_WARN;
            }
        }

        /* Applies what the prefs list and ends in the class scan. That scan is
           the first one from a process, which is what hands the keyboard and
           mouse of a ROM-started stack over from the boot classes to hid.class
           - so the classes have to be loaded before this point. */
        psdParseCfg();

        CloseLibrary(ps);
    } else {
        PutStr("Unable to open poseidon.library\n");
        ret = RETURN_FAIL;
    }

    FreeArgs(ArgsHook);
    return(ret);
}
