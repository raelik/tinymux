/*! \file flags.cpp
 * \brief Flag manipulation routines.
 *
 * $Id$
 *
 */

#include "copyright.h"
#include "autoconf.h"
#include "config.h"
#include "externs.h"

#include "command.h"
#include "powers.h"
#if defined(FIRANMUX)
#include "attrs.h"
#endif // FIRANMUX
#include "ansi.h"
#include "interface.h"

/* ---------------------------------------------------------------------------
 * fh_any: set or clear indicated bit, no security checking
 */

static bool fh_any(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    // Never let God drop his/her own wizbit.
    //
    if (  God(target)
       && reset
       && flag == WIZARD
       && fflags == FLAG_WORD1)
    {
        notify(player, T("You cannot make God mortal."));
        return false;
    }

    // Otherwise we can go do it.
    //
    if (reset)
    {
        db[target].fs.word[fflags] &= ~flag;
    }
    else
    {
        db[target].fs.word[fflags] |= flag;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * fh_god: only GOD may set or clear the bit
 */

static bool fh_god(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (!God(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_wiz: only WIZARDS (or GOD) may set or clear the bit
 */

static bool fh_wiz(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (!Wizard(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_wizroy: only WIZARDS, ROYALTY, (or GOD) may set or clear the bit
 */

static bool fh_wizroy(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (!WizRoy(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_restrict_player (renamed from fh_fixed): Only Wizards can set
 * * this on players, but ordinary players can set it on other types
 * * of objects.
 */
static bool fh_restrict_player
(
    dbref target,
    dbref player,
    FLAG flag,
    int fflags,
    bool reset
)
{
    if (  isPlayer(target)
       && !Wizard(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/* ---------------------------------------------------------------------------
 * fh_privileged: You can set this flag on a non-player object, if you
 * yourself have this flag and are a player who owns themselves (i.e.,
 * no robots). Only God can set this on a player.
 */
static bool fh_privileged
(
    dbref target,
    dbref player,
    FLAG flag,
    int fflags,
    bool reset
)
{
    if (!God(player))
    {
        if (  isPlayer(target)
#if !defined(FIRANMUX)
           || !isPlayer(player)
           || player != Owner(player)
#endif // FIRANMUX
           || (db[player].fs.word[fflags] & flag) == 0)
        {
            return false;
        }
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_inherit: only players may set or clear this bit.
 */

static bool fh_inherit(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (!Inherits(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_dark_bit: manipulate the dark bit. Nonwizards may not set on players.
 */

static bool fh_dark_bit(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (  !reset
       && isPlayer(target)
       && !(  (target == player)
           && Can_Hide(player))
       && !Wizard(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_going_bit: manipulate the going bit.  Non-gods may only clear.
 */

static bool fh_going_bit(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (  Going(target)
       && reset
       && (Typeof(target) != TYPE_GARBAGE))
    {
        notify(player, T("Your object has been spared from destruction."));
        return (fh_any(target, player, flag, fflags, reset));
    }
    if (!God(player))
    {
        return false;
    }

    // Even God should not be allowed set protected dbrefs GOING.
    //
    if (  !reset
       && (  target == 0
          || God(target)
          || target == mudconf.start_home
          || target == mudconf.start_room
          || target == mudconf.default_home
          || target == mudconf.master_room))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/*
 * ---------------------------------------------------------------------------
 * * fh_hear_bit: set or clear bits that affect hearing.
 */

static bool fh_hear_bit(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (isPlayer(target) && (flag & MONITOR))
    {
        if (Can_Monitor(player))
        {
            return (fh_any(target, player, flag, fflags, reset));
        }
        else
        {
            return false;
        }
    }

    bool could_hear = Hearer(target);
    bool result = fh_any(target, player, flag, fflags, reset);
    handle_ears(target, could_hear, Hearer(target));
    return result;
}


/* ---------------------------------------------------------------------------
 * fh_player_bit: Can set and reset this on everything but players.
 */
static bool fh_player_bit
(
    dbref target,
    dbref player,
    FLAG flag,
    int fflags,
    bool reset
)
{
    if (isPlayer(target))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}

/* ---------------------------------------------------------------------------
 * fh_staff: only STAFF, WIZARDS, ROYALTY, (or GOD) may set or clear
 * the bit.
 */
static bool fh_staff
(
    dbref target,
    dbref player,
    FLAG flag,
    int fflags,
    bool reset
)
{
    if (!Staff(player) && !God(player))
    {
        return false;
    }
    return (fh_any(target, player, flag, fflags, reset));
}


/* External reference to our telnet routine to resynch charset */
extern void SendCharsetRequest(DESC* d);

/*
 * ---------------------------------------------------------------------------
 * * fh_unicode: only players may set or clear this bit.
 */

static bool fh_unicode(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    if (!isPlayer(target))
    {
        return false;
    }

    if (fh_any(target, player, flag, fflags, reset))
    {
        DESC *dtemp;

        DESC_ITER_PLAYER(target, dtemp)
        {
            if (!reset)
            {
                if (CHARSET_UTF8 != dtemp->encoding)
                {
                    // Since we are changing to the UTF-8 character set, the
                    // printable state machine needs to be initialized.
                    //
                    dtemp->encoding = CHARSET_UTF8;
                    dtemp->raw_codepoint_state = CL_PRINT_START_STATE;
                }
            }
            else
            {
                dtemp->encoding = CHARSET_LATIN1;
            }

            if (  reset
               && OPTION_YES == dtemp->nvt_him_state[TELNET_CHARSET])
            {
                SendCharsetRequest(dtemp);
            }
        }
        return true;
    }
    return false;
}

/*
 * ---------------------------------------------------------------------------
 * * fh_ascii: only players may set or clear this bit.
 */

static bool fh_ascii(dbref target, dbref player, FLAG flag, int fflags, bool reset)
{
    bool result;

    if (!isPlayer(target))
    {
        return false;
    }
    result = fh_any(target, player, flag, fflags, reset);

    if (result)
    {
        DESC *dtemp;

        DESC_ITER_PLAYER(target, dtemp)
        {
            if (!reset)
                dtemp->encoding = CHARSET_ASCII;
            else
                dtemp->encoding = CHARSET_LATIN1;

            if (  reset
               && OPTION_YES == dtemp->nvt_him_state[TELNET_CHARSET])
            {
                SendCharsetRequest(dtemp);
            }
        }
    }

    return result;
}



static FLAGBITENT fbeAbode          = { ABODE,        'A',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeAnsi           = { ANSI,         'X',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeAudible        = { HEARTHRU,     'a',    FLAG_WORD1, 0,                    fh_hear_bit};
static FLAGBITENT fbeAuditorium     = { AUDITORIUM,   'b',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeBlind          = { BLIND,        'B',    FLAG_WORD2, 0,                    fh_wiz};
static FLAGBITENT fbeChownOk        = { CHOWN_OK,     'C',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeConnected      = { CONNECTED,    'c',    FLAG_WORD2, CA_NO_DECOMP,         fh_god};
static FLAGBITENT fbeDark           = { DARK,         'D',    FLAG_WORD1, 0,                    fh_dark_bit};
static FLAGBITENT fbeDestroyOk      = { DESTROY_OK,   'd',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeEnterOk        = { ENTER_OK,     'e',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeFixed          = { FIXED,        'f',    FLAG_WORD2, 0,                    fh_restrict_player};
static FLAGBITENT fbeFloating       = { FLOATING,     'F',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeGagged         = { GAGGED,       'j',    FLAG_WORD2, 0,                    fh_wiz};
static FLAGBITENT fbeGoing          = { GOING,        'G',    FLAG_WORD1, CA_NO_DECOMP,         fh_going_bit};
static FLAGBITENT fbeHalted         = { HALT,         'h',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeHasDaily       = { HAS_DAILY,    '*',    FLAG_WORD2, CA_GOD|CA_NO_DECOMP,  fh_god};
static FLAGBITENT fbeHasForwardList = { HAS_FWDLIST,  '&',    FLAG_WORD2, CA_GOD|CA_NO_DECOMP,  fh_god};
static FLAGBITENT fbeHasListen      = { HAS_LISTEN,   '@',    FLAG_WORD2, CA_GOD|CA_NO_DECOMP,  fh_god};
static FLAGBITENT fbeHasStartup     = { HAS_STARTUP,  '+',    FLAG_WORD1, CA_GOD|CA_NO_DECOMP,  fh_god};
static FLAGBITENT fbeHaven          = { HAVEN,        'H',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeHead           = { HEAD_FLAG,    '?',    FLAG_WORD2, 0,                    fh_wiz};
static FLAGBITENT fbeHtml           = { HTML,         '(',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeImmortal       = { IMMORTAL,     'i',    FLAG_WORD1, 0,                    fh_wiz};
static FLAGBITENT fbeInherit        = { INHERIT,      'I',    FLAG_WORD1, 0,                    fh_inherit};
static FLAGBITENT fbeJumpOk         = { JUMP_OK,      'J',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeKeepAlive      = { CKEEPALIVE,   'k',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeKey            = { KEY,          'K',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeLight          = { LIGHT,        'l',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeLinkOk         = { LINK_OK,      'L',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeMonitor        = { MONITOR,      'M',    FLAG_WORD1, 0,                    fh_hear_bit};
static FLAGBITENT fbeMyopic         = { MYOPIC,       'm',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeNoCommand      = { NO_COMMAND,   'n',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeAscii          = { ASCII    ,    '~',    FLAG_WORD2, 0,                    fh_ascii};
static FLAGBITENT fbeNoBleed        = { NOBLEED,      '-',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeNoSpoof        = { NOSPOOF,      'N',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeOpaque         = { TM_OPAQUE,    'O',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeOpenOk         = { OPEN_OK,      'z',    FLAG_WORD2, 0,                    fh_wiz};
static FLAGBITENT fbeParentOk       = { PARENT_OK,    'Y',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbePlayerMails    = { PLAYER_MAILS, ' ',    FLAG_WORD2, CA_GOD|CA_NO_DECOMP,  fh_god};
static FLAGBITENT fbePuppet         = { PUPPET,       'p',    FLAG_WORD1, 0,                    fh_hear_bit};
static FLAGBITENT fbeQuiet          = { QUIET,        'Q',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeRobot          = { ROBOT,        'r',    FLAG_WORD1, 0,                    fh_player_bit};
static FLAGBITENT fbeRoyalty        = { ROYALTY,      'Z',    FLAG_WORD1, 0,                    fh_wiz};
static FLAGBITENT fbeSafe           = { SAFE,         's',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeSlave          = { SLAVE,        'x',    FLAG_WORD2, CA_WIZARD,            fh_wiz};
static FLAGBITENT fbeStaff          = { STAFF,        'w',    FLAG_WORD2, 0,                    fh_wiz};
static FLAGBITENT fbeSticky         = { STICKY,       'S',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeSuspect        = { SUSPECT,      'u',    FLAG_WORD2, CA_WIZARD,            fh_wiz};
static FLAGBITENT fbeTerse          = { TERSE,        'q',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeTrace          = { TRACE,        'T',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeTransparent    = { SEETHRU,      't',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeUnfindable     = { UNFINDABLE,   'U',    FLAG_WORD2, 0,                    fh_any};
static FLAGBITENT fbeUnicode        = { UNICODE,      ' ',    FLAG_WORD3, CA_NO_DECOMP,         fh_unicode};
static FLAGBITENT fbeUninspected    = { UNINSPECTED,  'g',    FLAG_WORD2, 0,                    fh_wizroy};
static FLAGBITENT fbeVacation       = { VACATION,     '|',    FLAG_WORD2, 0,                    fh_restrict_player};
static FLAGBITENT fbeVerbose        = { VERBOSE,      'v',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeVisual         = { VISUAL,       'V',    FLAG_WORD1, 0,                    fh_any};
static FLAGBITENT fbeWizard         = { WIZARD,       'W',    FLAG_WORD1, 0,                    fh_god};
static FLAGBITENT fbeSitemon        = { SITEMON,      '$',    FLAG_WORD3, 0,                    fh_wiz};
#ifdef WOD_REALMS
static FLAGBITENT fbeFae            = { FAE,          '0',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeChimera        = { CHIMERA,      '1',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbePeering        = { PEERING,      '2',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeUmbra          = { UMBRA,        '3',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeShroud         = { SHROUD,       '4',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeMatrix         = { MATRIX,       '5',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeObf            = { OBF,          '6',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeHss            = { HSS,          '7',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeMedium         = { MEDIUM,       '8',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeDead           = { DEAD,         '9',    FLAG_WORD3, CA_STAFF,             fh_wizroy};
static FLAGBITENT fbeMarker0        = { MARK_0,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker1        = { MARK_1,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker2        = { MARK_2,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker3        = { MARK_3,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker4        = { MARK_4,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker5        = { MARK_5,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker6        = { MARK_6,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker7        = { MARK_7,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker8        = { MARK_8,       ' ',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker9        = { MARK_9,       ' ',    FLAG_WORD3, 0,                    fh_god};
#else // WOD_REALMS
static FLAGBITENT fbeMarker0        = { MARK_0,       '0',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker1        = { MARK_1,       '1',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker2        = { MARK_2,       '2',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker3        = { MARK_3,       '3',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker4        = { MARK_4,       '4',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker5        = { MARK_5,       '5',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker6        = { MARK_6,       '6',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker7        = { MARK_7,       '7',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker8        = { MARK_8,       '8',    FLAG_WORD3, 0,                    fh_god};
static FLAGBITENT fbeMarker9        = { MARK_9,       '9',    FLAG_WORD3, 0,                    fh_god};
#endif // WOD_REALMS
#if defined(FIRANMUX)
static FLAGBITENT fbeImmobile       = { IMMOBILE,     '#',    FLAG_WORD3, 0,                    fh_wiz};
static FLAGBITENT fbeLineWrap       = { LINEWRAP,     '>',    FLAG_WORD3, 0,                    fh_any};
static FLAGBITENT fbeQuell          = { QUELL,        ' ',    FLAG_WORD3, 0,                    fh_inherit};
static FLAGBITENT fbeRestricted     = { RESTRICTED,   '!',    FLAG_WORD3, CA_WIZARD,            fh_wiz};
static FLAGBITENT fbeParent         = { PARENT,       '+',    FLAG_WORD3, 0,                    fh_any};
#endif // FIRANMUX

FLAGNAMEENT gen_flag_names[] =
{
    {(UTF8 *)"ABODE",           true, &fbeAbode          },
    {(UTF8 *)"ACCENTS",        false, &fbeAscii          },
    {(UTF8 *)"ANSI",            true, &fbeAnsi           },
    {(UTF8 *)"ASCII",           true, &fbeAscii          },
    {(UTF8 *)"AUDIBLE",         true, &fbeAudible        },
    {(UTF8 *)"AUDITORIUM",      true, &fbeAuditorium     },
    {(UTF8 *)"BLEED",          false, &fbeNoBleed        },
    {(UTF8 *)"BLIND",           true, &fbeBlind          },
    {(UTF8 *)"COMMANDS",       false, &fbeNoCommand      },
    {(UTF8 *)"CHOWN_OK",        true, &fbeChownOk        },
    {(UTF8 *)"CONNECTED",       true, &fbeConnected      },
    {(UTF8 *)"DARK",            true, &fbeDark           },
    {(UTF8 *)"DESTROY_OK",      true, &fbeDestroyOk      },
    {(UTF8 *)"ENTER_OK",        true, &fbeEnterOk        },
    {(UTF8 *)"FIXED",           true, &fbeFixed          },
    {(UTF8 *)"FLOATING",        true, &fbeFloating       },
    {(UTF8 *)"GAGGED",          true, &fbeGagged         },
    {(UTF8 *)"GOING",           true, &fbeGoing          },
    {(UTF8 *)"HALTED",          true, &fbeHalted         },
    {(UTF8 *)"HAS_DAILY",       true, &fbeHasDaily       },
    {(UTF8 *)"HAS_FORWARDLIST", true, &fbeHasForwardList },
    {(UTF8 *)"HAS_LISTEN",      true, &fbeHasListen      },
    {(UTF8 *)"HAS_STARTUP",     true, &fbeHasStartup     },
    {(UTF8 *)"HAVEN",           true, &fbeHaven          },
    {(UTF8 *)"HEAD",            true, &fbeHead           },
    {(UTF8 *)"HTML",            true, &fbeHtml           },
    {(UTF8 *)"IMMORTAL",        true, &fbeImmortal       },
    {(UTF8 *)"INHERIT",         true, &fbeInherit        },
    {(UTF8 *)"JUMP_OK",         true, &fbeJumpOk         },
    {(UTF8 *)"KEEPALIVE",       true, &fbeKeepAlive      },
    {(UTF8 *)"KEY",             true, &fbeKey            },
    {(UTF8 *)"LIGHT",           true, &fbeLight          },
    {(UTF8 *)"LINK_OK",         true, &fbeLinkOk         },
    {(UTF8 *)"MARKER0",         true, &fbeMarker0        },
    {(UTF8 *)"MARKER1",         true, &fbeMarker1        },
    {(UTF8 *)"MARKER2",         true, &fbeMarker2        },
    {(UTF8 *)"MARKER3",         true, &fbeMarker3        },
    {(UTF8 *)"MARKER4",         true, &fbeMarker4        },
    {(UTF8 *)"MARKER5",         true, &fbeMarker5        },
    {(UTF8 *)"MARKER6",         true, &fbeMarker6        },
    {(UTF8 *)"MARKER7",         true, &fbeMarker7        },
    {(UTF8 *)"MARKER8",         true, &fbeMarker8        },
    {(UTF8 *)"MARKER9",         true, &fbeMarker9        },
    {(UTF8 *)"MONITOR",         true, &fbeMonitor        },
    {(UTF8 *)"MYOPIC",          true, &fbeMyopic         },
    {(UTF8 *)"NO_COMMAND",      true, &fbeNoCommand      },
    {(UTF8 *)"NOBLEED",         true, &fbeNoBleed        },
    {(UTF8 *)"NOSPOOF",         true, &fbeNoSpoof        },
    {(UTF8 *)"OPAQUE",          true, &fbeOpaque         },
    {(UTF8 *)"OPEN_OK",         true, &fbeOpenOk         },
    {(UTF8 *)"PARENT_OK",       true, &fbeParentOk       },
    {(UTF8 *)"PLAYER_MAILS",    true, &fbePlayerMails    },
    {(UTF8 *)"PUPPET",          true, &fbePuppet         },
    {(UTF8 *)"QUIET",           true, &fbeQuiet          },
    {(UTF8 *)"ROBOT",           true, &fbeRobot          },
    {(UTF8 *)"ROYALTY",         true, &fbeRoyalty        },
    {(UTF8 *)"SAFE",            true, &fbeSafe           },
    {(UTF8 *)"SITEMON",         true, &fbeSitemon        },
    {(UTF8 *)"SLAVE",           true, &fbeSlave          },
    {(UTF8 *)"SPOOF",          false, &fbeNoSpoof        },
    {(UTF8 *)"STAFF",           true, &fbeStaff          },
    {(UTF8 *)"STICKY",          true, &fbeSticky         },
    {(UTF8 *)"SUSPECT",         true, &fbeSuspect        },
    {(UTF8 *)"TERSE",           true, &fbeTerse          },
    {(UTF8 *)"TRACE",           true, &fbeTrace          },
    {(UTF8 *)"TRANSPARENT",     true, &fbeTransparent    },
    {(UTF8 *)"UNFINDABLE",      true, &fbeUnfindable     },
    {(UTF8 *)"UNICODE",         true, &fbeUnicode        },
    {(UTF8 *)"UNINSPECTED",     true, &fbeUninspected    },
    {(UTF8 *)"VACATION",        true, &fbeVacation       },
    {(UTF8 *)"VERBOSE",         true, &fbeVerbose        },
    {(UTF8 *)"VISUAL",          true, &fbeVisual         },
    {(UTF8 *)"WIZARD",          true, &fbeWizard         },
#ifdef WOD_REALMS
    {(UTF8 *)"FAE",             true, &fbeFae            },
    {(UTF8 *)"CHIMERA",         true, &fbeChimera        },
    {(UTF8 *)"PEERING",         true, &fbePeering        },
    {(UTF8 *)"UMBRA",           true, &fbeUmbra          },
    {(UTF8 *)"SHROUD",          true, &fbeShroud         },
    {(UTF8 *)"MATRIX",          true, &fbeMatrix         },
    {(UTF8 *)"OBF",             true, &fbeObf            },
    {(UTF8 *)"HSS",             true, &fbeHss            },
    {(UTF8 *)"MEDIUM",          true, &fbeMedium         },
    {(UTF8 *)"DEAD",            true, &fbeDead           },
#endif // WOD_REALMS
#if defined(FIRANMUX)
    {(UTF8 *)"IMMOBILE",        true, &fbeImmobile       },
    {(UTF8 *)"LINEWRAP",        true, &fbeLineWrap       },
    {(UTF8 *)"QUELL",           true, &fbeQuell          },
    {(UTF8 *)"RESTRICTED",      true, &fbeRestricted     },
    {(UTF8 *)"PARENT",          true, &fbeParent         },
#endif // FIRANMUX
    {(UTF8 *)NULL,             false, NULL}
};

OBJENT object_types[8] =
{
    {T("ROOM"),    'R', CA_PUBLIC, OF_CONTENTS|OF_EXITS|OF_DROPTO|OF_HOME},
    {T("THING"),   ' ', CA_PUBLIC, OF_CONTENTS|OF_LOCATION|OF_EXITS|OF_HOME|OF_SIBLINGS},
    {T("EXIT"),    'E', CA_PUBLIC, OF_SIBLINGS},
    {T("PLAYER"),  'P', CA_PUBLIC, OF_CONTENTS|OF_LOCATION|OF_EXITS|OF_HOME|OF_OWNER|OF_SIBLINGS},
    {T("TYPE5"),   '+', CA_GOD,    0},
    {T("GARBAGE"), '-', CA_PUBLIC, OF_CONTENTS|OF_LOCATION|OF_EXITS|OF_HOME|OF_SIBLINGS},
    {T("GARBAGE"), '#', CA_GOD,    0},
    {T("GARBAGE"), '=', CA_GOD,    0}
};

/* ---------------------------------------------------------------------------
 * init_flagtab: initialize flag hash tables.
 */

void init_flagtab(void)
{
    UTF8 *nbuf = alloc_sbuf("init_flagtab");
    for (FLAGNAMEENT *fp = gen_flag_names; fp->pOrigName; fp++)
    {
        fp->flagname = fp->pOrigName;
        mux_strncpy(nbuf, fp->pOrigName, SBUF_SIZE-1);
        mux_strlwr(nbuf);
        if (!hashfindLEN(nbuf, strlen((char *)nbuf), &mudstate.flags_htab))
        {
            hashaddLEN(nbuf, strlen((char *)nbuf), fp, &mudstate.flags_htab);
        }
    }
    free_sbuf(nbuf);
}

/* ---------------------------------------------------------------------------
 * display_flags: display available flags.
 */

void display_flagtab(dbref player)
{
    UTF8 *buf, *bp;
    FLAGNAMEENT *fp;

    bp = buf = alloc_lbuf("display_flagtab");
    safe_str(T("Flags:"), buf, &bp);
    for (fp = gen_flag_names; fp->flagname; fp++)
    {
        FLAGBITENT *fbe = fp->fbe;
        if (  (  (fbe->listperm & CA_WIZARD)
              && !Wizard(player))
           || (  (fbe->listperm & CA_GOD)
              && !God(player)))
        {
            continue;
        }
        safe_chr(' ', buf, &bp);
        safe_str(fp->flagname, buf, &bp);
        if (fbe->flaglett != ' ')
        {
            safe_chr('(', buf, &bp);
            if (!fp->bPositive)
            {
                safe_chr('!', buf, &bp);
            }
            safe_chr(fbe->flaglett, buf, &bp);
            safe_chr(')', buf, &bp);
        }
    }
    *bp = '\0';
    notify(player, buf);
    free_lbuf(buf);
}

UTF8 *MakeCanonicalFlagName
(
    const UTF8 *pName,
    int *pnName,
    bool *pbValid
)
{
    static UTF8 buff[SBUF_SIZE];
    UTF8 *p = buff;
    int nName = 0;

    while (*pName && nName < SBUF_SIZE)
    {
        *p = mux_tolower_ascii(*pName);
        p++;
        pName++;
        nName++;
    }
    *p = '\0';
    if (  0 < nName
       && nName < SBUF_SIZE)
    {
        *pnName = nName;
        *pbValid = true;
        return buff;
    }
    else
    {
        *pnName = 0;
        *pbValid = false;
        return NULL;
    }
}

static FLAGNAMEENT *find_flag(const UTF8 *flagname)
{
    // Convert flagname to canonical lowercase format.
    //
    int nName;
    bool bValid;
    UTF8 *pName = MakeCanonicalFlagName(flagname, &nName, &bValid);
    FLAGNAMEENT *fe = NULL;
    if (bValid)
    {
        fe = (FLAGNAMEENT *)hashfindLEN(pName, nName, &mudstate.flags_htab);
    }
    return fe;
}

// ---------------------------------------------------------------------------
// flag_set: Set or clear a specified flag on an object.
//
void flag_set(dbref target, dbref player, UTF8 *flag, int key)
{
    bool bDone = false;

    do
    {
        // Trim spaces, and handle the negation character.
        //
        while (mux_isspace(*flag))
        {
            flag++;
        }

        bool bNegate = false;
        if (*flag == '!')
        {
            bNegate = true;
            do
            {
                flag++;
            } while (mux_isspace(*flag));
        }

        // Beginning of flag name is now 'flag'.
        //
        UTF8 *nflag = flag;
        while (  *nflag != '\0'
              && !mux_isspace(*nflag))
        {
            nflag++;
        }

        if (*nflag == '\0')
        {
            bDone = true;
        }
        else
        {
            *nflag = '\0';
        }

        // Make sure a flag name was specified.
        //
        if (*flag == '\0')
        {
            if (bNegate)
            {
                notify(player, T("You must specify a flag to clear."));
            }
            else
            {
                notify(player, T("You must specify a flag to set."));
            }
        }
        else
        {
            FLAGNAMEENT *fp = find_flag(flag);
            if (!fp)
            {
                notify(player, T("I do not understand that flag."));
            }
            else
            {
                FLAGBITENT *fbe = fp->fbe;

                bool bClearSet = bNegate;
                if (!fp->bPositive)
                {
                    bNegate = !bNegate;
                }

                // Invoke the flag handler, and print feedback.
                //
                if (!fbe->handler(target, player, fbe->flagvalue, fbe->flagflag, bNegate))
                {
                    notify(player, NOPERM_MESSAGE);
                }
                else if (!(key & SET_QUIET) && !Quiet(player))
                {
                    notify(player, (bClearSet ? T("Cleared.") : T("Set.")));
                }
            }
        }
        flag = nflag + 1;

    } while (!bDone);
}

/*
 * ---------------------------------------------------------------------------
 * * decode_flags: converts a flags word into corresponding letters.
 */

UTF8 *decode_flags(dbref player, FLAGSET *fs)
{
    UTF8 *buf, *bp;
    buf = bp = alloc_sbuf("decode_flags");
    *bp = '\0';

    if (!Good_obj(player))
    {
        mux_strncpy(buf, T("#-2 ERROR"), SBUF_SIZE-1);
        return buf;
    }
    int flagtype = fs->word[FLAG_WORD1] & TYPE_MASK;
    bool bNeedColon = true;
    if (object_types[flagtype].lett != ' ')
    {
        safe_sb_chr(object_types[flagtype].lett, buf, &bp);
        bNeedColon = false;
    }

    FLAGNAMEENT *fp;
    for (fp = gen_flag_names; fp->flagname; fp++)
    {
        FLAGBITENT *fbe = fp->fbe;
        if (  !fp->bPositive
           || fbe->flaglett == ' ')
        {
            // Only look at positive-sense entries that have non-space flag
            // letters.
            //
            continue;
        }
        if (fs->word[fbe->flagflag] & fbe->flagvalue)
        {
            if (  (  (fbe->listperm & CA_STAFF)
                  && !Staff(player))
               || (  (fbe->listperm & CA_ADMIN)
                  && !WizRoy(player))
               || (  (fbe->listperm & CA_WIZARD)
                  && !Wizard(player))
               || (  (fbe->listperm & CA_GOD)
                  && !God(player)))
            {
                continue;
            }

            // Don't show CONNECT on dark wizards to mortals
            //
            if (  flagtype == TYPE_PLAYER
               && fbe->flagflag == FLAG_WORD2
               && fbe->flagvalue == CONNECTED
               && (fs->word[FLAG_WORD1] & (WIZARD | DARK)) == (WIZARD | DARK)
               && !See_Hidden(player))
            {
                continue;
            }

            if (  bNeedColon
               && mux_isdigit(fbe->flaglett))
            {
                // We can't allow numerical digits at the beginning.
                //
                safe_sb_chr(':', buf, &bp);
            }
            safe_sb_chr(fbe->flaglett, buf, &bp);
            bNeedColon = false;
        }
    }
    *bp = '\0';
    return buf;
}

/*
 * ---------------------------------------------------------------------------
 * * has_flag: does object have flag visible to player?
 */

bool has_flag(dbref player, dbref it, const UTF8 *flagname)
{
    FLAGNAMEENT *fp = find_flag(flagname);
    if (!fp)
    {
        return false;
    }
    FLAGBITENT *fbe = fp->fbe;

    if (  (  fp->bPositive
          && (db[it].fs.word[fbe->flagflag] & fbe->flagvalue))
       || (  !fp->bPositive
          && (db[it].fs.word[fbe->flagflag] & fbe->flagvalue) == 0))
    {
        if (  (  (fbe->listperm & CA_STAFF)
              && !Staff(player))
           || (  (fbe->listperm & CA_ADMIN)
              && !WizRoy(player))
           || (  (fbe->listperm & CA_WIZARD)
              && !Wizard(player))
           || (  (fbe->listperm & CA_GOD)
              && !God(player)))
        {
            return false;
        }

        // Don't show CONNECT on dark wizards to mortals
        //
        if (  isPlayer(it)
           && (fbe->flagvalue == CONNECTED)
           && (fbe->flagflag == FLAG_WORD2)
           && Hidden(it)
           && !See_Hidden(player))
        {
            return false;
        }
        return true;
    }
    return false;
}

/*
 * ---------------------------------------------------------------------------
 * * flag_description: Return an mbuf containing the type and flags on thing.
 */

UTF8 *flag_description(dbref player, dbref target)
{
    // Allocate the return buffer.
    //
    int otype = Typeof(target);
    UTF8 *buff = alloc_mbuf("flag_description");
    UTF8 *bp = buff;

    // Store the header strings and object type.
    //
    safe_mb_str(T("Type: "), buff, &bp);
    safe_mb_str(object_types[otype].name, buff, &bp);
    safe_mb_str(T(" Flags:"), buff, &bp);
    if (object_types[otype].perm != CA_PUBLIC)
    {
        *bp = '\0';
        return buff;
    }

    // Store the type-invariant flags.
    //
    FLAGNAMEENT *fp;
    for (fp = gen_flag_names; fp->flagname; fp++)
    {
        if (!fp->bPositive)
        {
            continue;
        }
        FLAGBITENT *fbe = fp->fbe;
        if (db[target].fs.word[fbe->flagflag] & fbe->flagvalue)
        {
            if (  (  (fbe->listperm & CA_STAFF)
                  && !Staff(player))
               || (  (fbe->listperm & CA_ADMIN)
                  && !WizRoy(player))
               || (  (fbe->listperm & CA_WIZARD)
                  && !Wizard(player))
               || (  (fbe->listperm & CA_GOD)
                  && !God(player)))
            {
                continue;
            }

            // Don't show CONNECT on dark wizards to mortals.
            //
            if (  isPlayer(target)
               && (fbe->flagvalue == CONNECTED)
               && (fbe->flagflag == FLAG_WORD2)
               && Hidden(target)
               && !See_Hidden(player))
            {
                continue;
            }
            safe_mb_chr(' ', buff, &bp);
            safe_mb_str(fp->flagname, buff, &bp);
        }
    }

    // Terminate the string, and return the buffer to the caller.
    //
    *bp = '\0';
    return buff;
}

/*
 * ---------------------------------------------------------------------------
 * * Return an lbuf containing the name and number of an object
 */

UTF8 *unparse_object_numonly(dbref target)
{
    UTF8 *buf = alloc_lbuf("unparse_object_numonly");
    if (target < 0)
    {
        mux_strncpy(buf, aszSpecialDBRefNames[-target], LBUF_SIZE-1);
    }
    else if (!Good_obj(target))
    {
        mux_sprintf(buf, LBUF_SIZE, "*ILLEGAL*(#%d)", target);
    }
    else
    {
        // Leave 100 bytes on the end for the dbref.
        //
        size_t vw;
        size_t nLen = ANSI_TruncateToField(Name(target), LBUF_SIZE-100,
            buf, LBUF_SIZE, &vw);
        UTF8 *bp = buf + nLen;

        safe_str(T("(#"), buf, &bp);
        safe_ltoa(target, buf, &bp);
        safe_chr(')', buf, &bp);
        *bp = '\0';
    }
    return buf;
}

#if defined(FIRANMUX)
static bool AcquireColor(dbref player, dbref target, UTF8 SimplifiedCodes[8])
{
    int   aflags;
    dbref aowner;

    // Get the value of the object's '@color' attribute (or on a parent).
    //
    UTF8 *color_attr = alloc_lbuf("AcquireColor.1");
    atr_pget_str(color_attr, target, A_COLOR, &aowner, &aflags);

    if ('\0' == color_attr[0])
    {
        free_lbuf(color_attr);
        return false;
    }
    else
    {
        UTF8 *AnsiCodes = alloc_lbuf("AcquireColor.2");
        UTF8 *ac = AnsiCodes;
        mux_exec(color_attr, AnsiCodes, &ac, player, target, target,
                AttrTrace(aflags, EV_EVAL|EV_TOP|EV_FCHECK), NULL, 0);
        *ac = '\0';
        free_lbuf(color_attr);

        SimplifyColorLetters(SimplifiedCodes, AnsiCodes);
        free_lbuf(AnsiCodes);
        return true;
    }
}
#endif // FIRANMUX

/*
 * ---------------------------------------------------------------------------
 * * Return an lbuf pointing to the object name and possibly the db# and flags
 */
UTF8 *unparse_object(dbref player, dbref target, bool obey_myopic, bool bAddColor)
{
    UTF8 *buf = alloc_lbuf("unparse_object");
    if (NOPERM <= target && target < 0)
    {
        mux_strncpy(buf, aszSpecialDBRefNames[-target], LBUF_SIZE-1);
    }
    else if (!Good_obj(target))
    {
        mux_sprintf(buf, LBUF_SIZE, "*ILLEGAL*(#%d)", target);
    }
    else
    {
        bool exam;
        if (obey_myopic)
        {
            exam = MyopicExam(player, target);
        }
        else
        {
            exam = Examinable(player, target);
        }

        // Leave and extra 100 bytes for the dbref and flags at the end and
        // color at the beginning if necessary..
        //
        size_t vw;
        size_t nLen = ANSI_TruncateToField(Moniker(target), LBUF_SIZE-100,
            buf, LBUF_SIZE, &vw);

        UTF8 *bp = buf + nLen;

#if defined(FIRANMUX)
        if (  vw == nLen
           && bAddColor)
        {
            // There is no color in the name, so look for @color, or highlight.
            //
            UTF8 *buf2 = alloc_lbuf("unparse_object.color");
            UTF8 *bp2  = buf2;

            UTF8 SimplifiedCodes[8];
            if (AcquireColor(player, target, SimplifiedCodes))
            {
                for (int i = 0; SimplifiedCodes[i]; i++)
                {
                    const UTF8 *pColor = ColorTable[(unsigned char)SimplifiedCodes[i]];
                    if (pColor)
                    {
                        safe_str(pColor, buf2, &bp2);
                    }
                }
            }
            else
            {
                safe_str((UTF8 *)COLOR_INTENSE, buf2, &bp2);
            }

            *bp = '\0';
            safe_str(buf, buf2, &bp2);
            safe_str((UTF8 *)COLOR_RESET, buf2, &bp2);

            // Swap buffers.
            //
            free_lbuf(buf);
            buf = buf2;
            bp  = bp2;
        }
#else
        UNUSED_PARAMETER(bAddColor);
#endif // FIRANMUX

        if (  exam
           || (Flags(target) & (CHOWN_OK | JUMP_OK | LINK_OK | DESTROY_OK))
           || (Flags2(target) & ABODE))
        {
            // Show everything.
            //
            UTF8 *fp = decode_flags(player, &(db[target].fs));

            safe_str(T("(#"), buf, &bp);
            safe_ltoa(target, buf, &bp);
            safe_str(fp, buf, &bp);
            safe_chr(')', buf, &bp);

            free_sbuf(fp);
        }
        *bp = '\0';
    }
    return buf;
}


/* ---------------------------------------------------------------------------
 * cf_flag_access: Modify who can set a flag.
 */

CF_HAND(cf_flag_access)
{
    UNUSED_PARAMETER(vp);
    UNUSED_PARAMETER(pExtra);
    UNUSED_PARAMETER(nExtra);

    MUX_STRTOK_STATE tts;
    mux_strtok_src(&tts, str);
    mux_strtok_ctl(&tts, T(" \t=,"));
    UTF8 *fstr = mux_strtok_parse(&tts);
    UTF8 *permstr = mux_strtok_parse(&tts);

    if (!fstr || !*fstr)
    {
        return -1;
    }

    FLAGNAMEENT *fp;
    if ((fp = find_flag(fstr)) == NULL)
    {
        cf_log_notfound(player, cmd, T("No such flag"), fstr);
        return -1;
    }
    FLAGBITENT *fbe = fp->fbe;

    // Don't change the handlers on special things.
    //
    if (  (fbe->handler != fh_any)
       && (fbe->handler != fh_wizroy)
       && (fbe->handler != fh_wiz)
       && (fbe->handler != fh_god)
       && (fbe->handler != fh_restrict_player)
       && (fbe->handler != fh_privileged))
    {
        STARTLOG(LOG_CONFIGMODS, "CFG", "PERM");
        log_text(T("Cannot change access for flag: "));
        log_text(fp->flagname);
        ENDLOG;
        return -1;
    }

    if (!strcmp((char *)permstr, "any"))
    {
        fbe->handler = fh_any;
    }
    else if (!strcmp((char *)permstr, "royalty"))
    {
        fbe->handler = fh_wizroy;
    }
    else if (!strcmp((char *)permstr, "wizard"))
    {
        fbe->handler = fh_wiz;
    }
    else if (!strcmp((char *)permstr, "god"))
    {
        fbe->handler = fh_god;
    }
    else if (!strcmp((char *)permstr, "restrict_player"))
    {
        fbe->handler = fh_restrict_player;
    }
    else if (!strcmp((char *)permstr, "privileged"))
    {
        fbe->handler = fh_privileged;
    }
    else if (!strcmp((char *)permstr, "staff"))
    {
        fbe->handler = fh_staff;
    }
    else
    {
        cf_log_notfound(player, cmd, T("Flag access"), permstr);
        return -1;
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------
 * * convert_flags: convert a list of flag letters into its bit pattern.
 * * Also set the type qualifier if specified and not already set.
 */

bool convert_flags(dbref player, UTF8 *flaglist, FLAGSET *fset, FLAG *p_type)
{
    FLAG type = NOTYPE;
    FLAGSET flagmask;
    memset(&flagmask, 0, sizeof(flagmask));
    int i;

    UTF8 *s;
    bool handled;
    for (s = flaglist; *s; s++)
    {
        handled = false;

        // Check for object type.
        //
        for (i = 0; i <= 7 && !handled; i++)
        {
            if (  object_types[i].lett == *s
               && !(  (  (object_types[i].perm & CA_STAFF)
                      && !Staff(player))
                   || (  (object_types[i].perm & CA_ADMIN)
                      && !WizRoy(player))
                   || (  (object_types[i].perm & CA_WIZARD)
                      && !Wizard(player))
                   || (  (object_types[i].perm & CA_GOD)
                      && !God(player))))
            {
                if (  type != NOTYPE
                   && type != i)
                {
                    UTF8 *p = tprintf("%c: Conflicting type specifications.",
                        *s);
                    notify(player, p);
                    return false;
                }
                type = i;
                handled = true;
            }
        }

        // Check generic flags.
        //
        if (handled)
        {
            continue;
        }
        FLAGNAMEENT *fp;
        for (fp = gen_flag_names; fp->flagname && !handled; fp++)
        {
            FLAGBITENT *fbe = fp->fbe;
            if (  !fp->bPositive
               || fbe->flaglett == ' ')
            {
                continue;
            }
            if (  fbe->flaglett == *s
               && !(  (  (fbe->listperm & CA_STAFF)
                      && !Staff(player))
                   || (  (fbe->listperm & CA_ADMIN)
                      && !WizRoy(player))
                   || (  (fbe->listperm & CA_WIZARD)
                      && !Wizard(player))
                   || (  (fbe->listperm & CA_GOD)
                      && !God(player))))
            {
                flagmask.word[fbe->flagflag] |= fbe->flagvalue;
                handled = true;
            }
        }

        if (!handled)
        {
            notify(player,
                   tprintf("%c: Flag unknown or not valid for specified object type",
                       *s));
            return false;
        }
    }

    // Return flags to search for and type.
    //
    *fset = flagmask;
    *p_type = type;
    return true;
}

/*
 * ---------------------------------------------------------------------------
 * * decompile_flags: Produce commands to set flags on target.
 */

void decompile_flags(dbref player, dbref thing, UTF8 *thingname)
{
    // Report generic flags.
    //
    FLAGNAMEENT *fp;
    for (fp = gen_flag_names; fp->flagname; fp++)
    {
        FLAGBITENT *fbe = fp->fbe;

        // Only handle positive-sense entries.
        // Skip if we shouldn't decompile this flag.
        // Skip if this flag isn't set.
        // Skip if we can't see this flag.
        //
        if (  !fp->bPositive
           || (fbe->listperm & CA_NO_DECOMP)
           || (db[thing].fs.word[fbe->flagflag] & fbe->flagvalue) == 0
           || !check_access(player, fbe->listperm))
        {
            continue;
        }

        // Report this flag.
        //
        notify(player, tprintf("@set %s=%s", thingname, fp->flagname));
    }
}

// do_flag: Rename flags or remove flag aliases.
// Based on RhostMUSH code.
//
static bool flag_rename(UTF8 *alias, UTF8 *newname)
{
    int nAlias;
    bool bValidAlias;
    UTF8 *pCheckedAlias = MakeCanonicalFlagName(alias, &nAlias, &bValidAlias);
    if (!bValidAlias)
    {
        return false;
    }
    UTF8 *pAlias = alloc_sbuf("flag_rename.old");
    memcpy(pAlias, pCheckedAlias, nAlias+1);

    int nNewName;
    bool bValidNewName;
    UTF8 *pCheckedNewName = MakeCanonicalFlagName(newname, &nNewName, &bValidNewName);
    if (!bValidNewName)
    {
        free_sbuf(pAlias);
        return false;
    }
    UTF8 *pNewName = alloc_sbuf("flag_rename.new");
    memcpy(pNewName, pCheckedNewName, nNewName+1);

    FLAGNAMEENT *flag1;
    flag1 = (FLAGNAMEENT *)hashfindLEN(pAlias, nAlias, &mudstate.flags_htab);
    if (flag1 != NULL)
    {
        FLAGNAMEENT *flag2;
        flag2 = (FLAGNAMEENT *)hashfindLEN(pNewName, nNewName, &mudstate.flags_htab);
        if (flag2 == NULL)
        {
            hashaddLEN(pNewName, nNewName, flag1, &mudstate.flags_htab);

            if (flag1->flagname != flag1->pOrigName)
            {
                MEMFREE(flag1->flagname);
            }
            mux_strupr(pNewName);
            flag1->flagname = StringCloneLen(pNewName, nNewName);

            free_sbuf(pAlias);
            free_sbuf(pNewName);
            return true;
        }
    }
    free_sbuf(pAlias);
    free_sbuf(pNewName);
    return false;
}

void do_flag(dbref executor, dbref caller, dbref enactor, int key, int nargs,
             UTF8 *flag1, UTF8 *flag2)
{
    UNUSED_PARAMETER(caller);
    UNUSED_PARAMETER(enactor);

    if (key & FLAG_REMOVE)
    {
        if (nargs == 2)
        {
            notify(executor, T("Extra argument ignored."));
        }
        int nAlias;
        bool bValidAlias;
        UTF8 *pCheckedAlias = MakeCanonicalFlagName(flag1, &nAlias, &bValidAlias);
        if (bValidAlias)
        {
            FLAGNAMEENT *lookup;
            lookup = (FLAGNAMEENT *)hashfindLEN(pCheckedAlias, nAlias, &mudstate.flags_htab);
            if (lookup)
            {
                if (  lookup->flagname != lookup->pOrigName
                   && mux_stricmp(lookup->flagname, pCheckedAlias) == 0)
                {
                    MEMFREE(lookup->flagname);
                    lookup->flagname = lookup->pOrigName;
                    hashdeleteLEN(pCheckedAlias, nAlias, &mudstate.flags_htab);
                    notify(executor, tprintf("Flag name '%s' removed from the hash table.", pCheckedAlias));
                }
                else
                {
                    notify(executor, T("Error: You can't remove the present flag name from the hash table."));
                }
            }
        }
    }
    else
    {
        if (nargs < 2)
        {
            notify(executor, T("You must specify a flag and a name."));
            return;
        }
        if (flag_rename(flag1, flag2))
        {
            notify(executor, T("Flag name changed."));
        }
        else
        {
            notify(executor, T("Error: Bad flagname given or flag not found."));
        }
    }
}

/* ---------------------------------------------------------------------------
 * cf_flag_name: Rename a flag. Counterpart to @flag.
 */

CF_HAND(cf_flag_name)
{
    UNUSED_PARAMETER(vp);
    UNUSED_PARAMETER(pExtra);
    UNUSED_PARAMETER(nExtra);
    UNUSED_PARAMETER(player);
    UNUSED_PARAMETER(cmd);

    MUX_STRTOK_STATE tts;
    mux_strtok_src(&tts, str);
    mux_strtok_ctl(&tts, T(" \t=,"));
    UTF8 *flagstr = mux_strtok_parse(&tts);
    UTF8 *namestr = mux_strtok_parse(&tts);

    if (  !flagstr
       || !*flagstr
       || !namestr
       || !*namestr)
    {
        return -1;
    }

    if (flag_rename(flagstr, namestr))
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
