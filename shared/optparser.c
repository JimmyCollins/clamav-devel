/*
 *  Copyright (C) 2008 Sourcefire, Inc.
 *
 *  Author: Tomasz Kojm <tkojm@clamav.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/*
 * TODO:
 * - freshclam, clamscan, clamdscan, clamconf, milter
 * - clamconf: generation/verification/updating of config files and man page entries
 * - automatically generate --help pages (use the first line from the description)
 */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>

#include "shared/optparser.h"
#include "shared/misc.h"

#include "libclamav/regex/regex.h"

#include "getopt.h"

#define MAXCMDOPTS  64
#define MAX(a,b) (a > b ? a : b)

#define MATCH_NUMBER "^[0-9]+$"
#define MATCH_SIZE "^[0-9]+[kKmM]?$"
#define MATCH_BOOL "^([yY]es|[tT]rue|1|[nN]o|[fF]alse|0)$"

static const struct clam_option {
    const char *name;
    const char *longopt;
    char shortopt;
    unsigned short argtype;
    const char *regex;
    int numarg;
    const char *strarg;
    short multiple;
    unsigned short owner;
    const char *description;
    const char *suggested;
} clam_options[] = {
    /* name,   longopt, sopt, argtype, regex, num, str, mul, owner, description, suggested */

    /* cmdline only */
    { NULL, "help", 'h', OPT_BOOL, NULL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM, "", "" },
    { NULL, "config-file", 'c', OPT_STRING, NULL, 0, CONFDIR"/clamd.conf", 0, OPT_CLAMD, "", "" },
    { NULL, "config-file", 0, OPT_STRING, NULL, 0, CONFDIR"/freshclam.conf", 0, OPT_FRESHCLAM, "", "" },
    { NULL, "version", 'V', OPT_BOOL, NULL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM, "", "" },
    { NULL, "debug", 0, OPT_BOOL, NULL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM, "", "" },
    { NULL, "verbose", 'v', OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "quiet", 0, OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "no-warnings", 0, OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "stdout", 0, OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "daemon", 'd', OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "no-dns", 0, OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "http-proxy", 0, OPT_STRING, NULL, 0, NULL, 0, OPT_FRESHCLAM | OPT_DEPRECATED, "", "" },
    { NULL, "proxy-user", 0, OPT_STRING, NULL, 0, NULL, 0, OPT_FRESHCLAM | OPT_DEPRECATED, "", "" },
    { NULL, "list-mirrors", 0, OPT_BOOL, NULL, 0, NULL, 0, OPT_FRESHCLAM, "", "" },
    { NULL, "submit-stats", 0, OPT_STRING, NULL, 0, CONFDIR"/clamd.conf", 0, OPT_FRESHCLAM, "", "" }, /* Don't merge this one with SubmitDetectionStats */

    /* config file/cmdline options */
    { "LogFile", "log", 'l', OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD | OPT_MILTER, "Save all reports to a log file.", "/tmp/clamav.log" },

    { "LogFileUnlock", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_MILTER, "By default the log file is locked for writing and only a single\ndaemon process can write to it. This option disables the lock.", "no" },

    { "LogFileMaxSize", NULL, 0, OPT_SIZE, MATCH_SIZE, 1048576, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Maximum size of the log file.\nValue of 0 disables the limit.", "5M" },

    { "LogTime", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Log time with each message.", "yes" },

    { "LogClean", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Log all clean files.\nUseful in debugging but drastically increases the log size.", "no" },

    { "LogVerbose", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Enable verbose logging.", "no" },

    { "LogSyslog", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Use the system logger (can work together with LogFile).", "yes" },

    { "LogFacility", NULL, 0, OPT_STRING, NULL, -1, "LOG_LOCAL6", 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Type of syslog messages.\nPlease refer to 'man syslog' for the facility names.", "LOG_MAIL" },

    { "PidFile", "pid", 'p', OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Save the process ID to a file.", "/var/run/clamd.pid" },

    { "TemporaryDirectory", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD | OPT_MILTER, "This option allows you to change the default temporary directory.", "/tmp" },

    { "DatabaseDirectory", "datadir", 0, OPT_STRING, NULL, -1, DATADIR, 0, OPT_CLAMD | OPT_FRESHCLAM, "This option allows you to change the default database directory.\nIf you enable it, please make sure it points to the same directory in\nboth clamd and freshclam.", "/var/lib/clamav" },

    { "LocalSocket", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD, "Path to a local socket file the daemon will listen on.", "/tmp/clamd.socket" },

    { "FixStaleSocket", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD | OPT_MILTER, "Remove a stale socket after unclean shutdown", "yes" },

    { "TCPSocket", NULL, 0, OPT_NUMBER, MATCH_NUMBER, -1, NULL, 0, OPT_CLAMD, "A TCP port number the daemon will listen on.", "3310" },

    /* FIXME: add a regex for IP addr */
    { "TCPAddr", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD, "By default clamd binds to INADDR_ANY.\nThis option allows you to restrict the TCP address and provide\nsome degree of protection from the outside world.", "3310" },

    { "MaxConnectionQueueLength", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 15, NULL, 0, OPT_CLAMD, "Maximum length the queue of pending connections may grow to.", "30" },

    { "StreamMaxLength", NULL, 0, OPT_SIZE, MATCH_SIZE, 10485760, NULL, 0, OPT_CLAMD, "Close the STREAM session when the data size limit is exceeded.\nThe value should match your MTA's limit for the maximum attachment size.", "25M" },

    { "StreamMinPort", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 1024, NULL, 0, OPT_CLAMD, "The STREAM command uses an FTP-like protocol.\nThis option sets the lower boundary for the port range.", "1024" },

    { "StreamMaxPort", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 2048, NULL, 0, OPT_CLAMD, "This option sets the upper boundary for the port range.", "2048" },

    { "MaxThreads", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 10, NULL, 0, OPT_CLAMD | OPT_MILTER, "Maximum number of threads running at the same time.", "20" },

    { "ReadTimeout", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 120, NULL, 0, OPT_CLAMD | OPT_MILTER, "This option specifies the time (in seconds) after which clamd should\ntimeout if a client doesn't provide any data.", "120" },

    { "IdleTimeout", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 30, NULL, 0, OPT_CLAMD, "This option specifies how long (in seconds) the process should wait for a new job.", "60" },

    { "ExcludePath", NULL, 0, OPT_STRING, NULL, -1, NULL, 1, OPT_CLAMD, "Don't scan files/directories whose names match the provided\nregular expression. This option can be specified multiple times.", "^/proc/" },

    { "MaxDirectoryRecursion", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 15, NULL, 0, OPT_CLAMD, "Maximum depth the directories are scanned at.", "15" },

    { "FollowDirectorySymlinks", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Follow directory symlinks.", "no" },

    { "FollowFileSymlinks", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Follow symlinks to regular files.", "no" },

    { "SelfCheck", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 600, NULL, 0, OPT_CLAMD, "This option specifies the time intervals (in seconds) in which clamd\nshould perform a database check.", "600" },

    { "VirusEvent", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD, "Execute a command when a virus is found. In the command string %v will be\nreplaced with the virus name. Additionally, two environment variables will\nbe defined: $CLAM_VIRUSEVENT_FILENAME and $CLAM_VIRUSEVENT_VIRUSNAME.", "/usr/bin/mailx -s \"ClamAV VIRUS ALERT: %v\" alert < /dev/null" },

    { "ExitOnOOM", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Stop the daemon when libclamav reports an out of memory condition.", "yes" },

    { "Foreground", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Don't fork into background.", "no" },

    { "Debug", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM, "Enable debug messages in libclamav.", "no" },

    { "LeaveTemporaryFiles", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Don't remove temporary files (for debugging purposes).", "no" },

    { "User", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_CLAMD | OPT_MILTER, "Run the daemon as a specified user (the process must be started by root).", "clamav" },

    { "AllowSupplementaryGroups", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD | OPT_FRESHCLAM | OPT_MILTER, "Initialize a supplementary group access (the process must be started by root).", "no" },

    /* Scan options */

    { "DetectPUA", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Detect Potentially Unwanted Applications.", "yes" },

    { "ExcludePUA", NULL, 0, OPT_STRING, NULL, -1, NULL, 1, OPT_CLAMD, "Exclude a specific PUA category. This directive can be used multiple times.\nSee http://www.clamav.net/support/pua for the complete list of PUA\ncategories.", "NetTool" },

    { "IncludePUA", NULL, 0, OPT_STRING, NULL, -1, NULL, 1, OPT_CLAMD, "Only include a specific PUA category. This directive can be used multiple\ntimes.", "Spy" },

    { "AlgorithmicDetection", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "In some cases (eg. complex malware, exploits in graphic files, and others),\nClamAV uses special algorithms to provide accurate detection. This option\ncontrols the algorithmic detection.", "yes" },

    { "ScanPE", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "PE stands for Portable Executable - it's an executable file format used\nin all 32- and 64-bit versions of Windows operating systems. This option\nallows ClamAV to perform a deeper analysis of executable files and it's also\nrequired for decompression of popular executable packers such as UPX or FSG.", "yes" },

    { "ScanELF", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "Executable and Linking Format is a standard format for UN*X executables.\nThis option allows you to control the scanning of ELF files.", "yes" },

    { "DetectBrokenExecutables", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "With this option enabled clamav will try to detect broken executables\n(both PE and ELF) and mark them as Broken.Executable.", "yes" },

    { "ScanMail", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "Enable the built in email scanner.", "yes" },

    { "MailFollowURLs", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "If an email contains URLs ClamAV can download and scan them.\nWARNING: This option may open your system to a DoS attack. Please don't use\nthis feature on highly loaded servers.", "no" },

    { "ScanPartialMessages", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Scan RFC1341 messages split over many emails. You will need to\nperiodically clean up $TemporaryDirectory/clamav-partial directory.\nWARNING: This option may open your system to a DoS attack. Please don't use\nthis feature on highly loaded servers.", "no" },

    { "PhishingSignatures", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "With this option enabled ClamAV will try to detect phishing attempts by using\nsignatures.", "yes" },

    { "PhishingScanURLs", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "Scan URLs found in mails for phishing attempts using heuristics.", "yes" },

    { "PhishingAlwaysBlockCloak", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Always block cloaked URLs, even if they're not in the database.\nThis feature can lead to false positives.", "no" },

    { "PhishingAlwaysBlockSSLMismatch", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Always block SSL mismatches in URLs, even if they're not in the database.\nThis feature can lead to false positives.", "" },

    { "HeuristicScanPrecedence", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Allow heuristic match to take precedence.\nWhen enabled, if a heuristic scan (such as phishingScan) detects\na possible virus/phish it will stop scan immediately. Recommended, saves CPU\nscan-time.\nWhen disabled, virus/phish detected by heuristic scans will be reported only\nat the end of a scan. If an archive contains both a heuristically detected\nvirus/phish, and a real malware, the real malware will be reported.\nKeep this disabled if you intend to handle \"*.Heuristics.*\" viruses\ndifferently from \"real\" malware.\nIf a non-heuristically-detected virus (signature-based) is found first,\nthe scan is interrupted immediately, regardless of this config option.", "yes" },

    { "StructuredDataDetection", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Enable the Data Loss Prevention module.", "no" },

    { "StructuredMinCreditCardCount", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 3, NULL, 0, OPT_CLAMD, "This option sets the lowest number of Credit Card numbers found in a file\nto generate a detect.", "5" },

    { "StructuredMinSSNCount", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 3, NULL, 0, OPT_CLAMD, "This option sets the lowest number of Social Security Numbers found\nin a file to generate a detect.", "5" },

    { "StructuredSSNFormatNormal", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "With this option enabled the DLP module will search for valid\nSSNs formatted as xxx-yy-zzzz.", "yes" },

    { "StructuredSSNFormatStripped", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "With this option enabled the DLP module will search for valid\nSSNs formatted as xxxyyzzzz", "no" },

    { "ScanHTML", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "Perform HTML/JavaScript/ScriptEncoder normalisation and decryption.", "yes" },

    { "ScanOLE2", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "This option enables scanning of OLE2 files, such as Microsoft Office\ndocuments and .msi files.", "yes" },

    { "ScanPDF", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "This option enables scanning within PDF files.", "yes" },

    { "ScanArchive", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_CLAMD, "Scan within archives and compressed files.", "yes" },

    { "ArchiveBlockEncrypted", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_CLAMD, "Mark encrypted archives as viruses (Encrypted.Zip, Encrypted.RAR).", "no" },

    { "MaxScanSize", NULL, 0, OPT_SIZE, MATCH_SIZE, -1, NULL, 0, OPT_CLAMD, "This option sets the maximum amount of data to be scanned for each input file.\nArchives and other containers are recursively extracted and scanned up to this\nvalue.\nThe value of 0 disables the limit.\nWARNING: disabling this limit or setting it too high may result in severe damage.", "100M" },

    { "MaxFileSize", NULL, 0, OPT_SIZE, MATCH_SIZE, -1, NULL, 0, OPT_CLAMD | OPT_MILTER, "Files larger than this limit won't be scanned. Affects the input file itself\nas well as files contained inside it (when the input file is an archive, a\ndocument or some other kind of container).\nThe value of 0 disables the limit.\nWARNING: disabling this limit or setting it too high may result in severe damage to the system.", "25M" },

    { "MaxRecursion", NULL, 0, OPT_NUMBER, MATCH_NUMBER, -1, NULL, 0, OPT_CLAMD, "Nested archives are scanned recursively, e.g. if a Zip archive contains a RAR\nfile, all files within it will also be scanned. This option specifies how\ndeeply the process should be continued.\nThe value of 0 disables the limit.\nWARNING: disabling this limit or setting it too high may result in severe damage to the system.", "16" },

    { "MaxFiles", NULL, 0, OPT_NUMBER, MATCH_NUMBER, -1, NULL, 0, OPT_CLAMD, "Number of files to be scanned within an archive, a document, or any other\ncontainer file.\nThe value of 0 disables the limit.\nWARNING: disabling this limit or setting it too high may result in severe damage to the system.", "10000" },

    { "ClamukoScanOnAccess", NULL, 0, OPT_BOOL, MATCH_BOOL, -1, NULL, 0, OPT_CLAMD, "This option enables Clamuko. Dazuko needs to be already configured and\nrunning.", "no" },

    { "ClamukoScanOnOpen", NULL, 0, OPT_BOOL, MATCH_BOOL, -1, NULL, 0, OPT_CLAMD, "Scan files when they get opened by the system.", "yes" },

    { "ClamukoScanOnClose", NULL, 0, OPT_BOOL, MATCH_BOOL, -1, NULL, 0, OPT_CLAMD, "Scan files when they get closed by the system.", "yes" },

    { "ClamukoScanOnExec", NULL, 0, OPT_BOOL, MATCH_BOOL, -1, NULL, 0, OPT_CLAMD, "Scan files when they get executed by the system.", "yes" },

    { "ClamukoIncludePath", NULL, 0, OPT_STRING, NULL, -1, NULL, 1, OPT_CLAMD, "This option specifies a directory (together will all files and directories\ninside this directory) which should be scanned on-access. This option can\nbe used multiple times.", "/home" },

    { "ClamukoExcludePath", NULL, 0, OPT_STRING, NULL, -1, NULL, 1, OPT_CLAMD, "This option allows excluding directories from on-access scanning. It can be used multiple times.", "/home/bofh" },

    { "ClamukoMaxFileSize", NULL, 0, OPT_SIZE, MATCH_SIZE, 5242880, NULL, 0, OPT_CLAMD, "Files larger than this value will not be scanned.", "5M" },

    /* FIXME: mark these as private and don't output into clamd.conf/man */
    { "DevACOnly", NULL, 0, OPT_BOOL, MATCH_BOOL, -1, NULL, 0, OPT_CLAMD, "", "" },

    { "DevACDepth", NULL, 0, OPT_NUMBER, MATCH_NUMBER, -1, NULL, 0, OPT_CLAMD, "", "" },

    /* Freshclam-only entries */

    /* FIXME: drop this entry and use LogFile */
    { "UpdateLogFile", "log", 'l', OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "Save all reports to a log file.", "/var/log/freshclam.log" },

    { "DatabaseOwner", "user", 'u', OPT_STRING, NULL, -1, CLAMAVUSER, 0, OPT_FRESHCLAM, "When started by root freshclam will drop privileges and switch to the user\ndefined in this option.", CLAMAVUSER },

    { "Checks", "checks", 'c', OPT_NUMBER, MATCH_NUMBER, 12, NULL, 0, OPT_FRESHCLAM, "This option defined how many times daily freshclam should check for\na database update.", "24" },

    { "DNSDatabaseInfo", NULL, 0, OPT_STRING, NULL, -1, "current.cvd.clamav.net", 0, OPT_FRESHCLAM, "Use DNS to verify the virus database version. Freshclam uses DNS TXT records\nto verify the versions of the database and software itself. With this\ndirective you can change the database verification domain.\nWARNING: Please don't change it unless you're configuring freshclam to use\nyour own database verification domain.", "current.cvd.clamav.net" },

    /* FIXME: - add an inactive entry for db.XY.clamav.net for freshclam.conf
     * purposes
     * - 
     */
    { "DatabaseMirror", NULL, 0, OPT_STRING, NULL, -1, NULL, 1, OPT_FRESHCLAM, "FIXME", "" },

    { "MaxAttempts", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 3, NULL, 0, OPT_FRESHCLAM, "This option defines how many attempts freshclam should make before giving up.", "5" },

    { "ScriptedUpdates", NULL, 0, OPT_BOOL, MATCH_BOOL, 1, NULL, 0, OPT_FRESHCLAM, "With this option you can control scripted updates. It's highly recommended to keep them enabled.", "yes" },

    { "CompressLocalDatabase", NULL, 0, OPT_BOOL, MATCH_BOOL, 0, NULL, 0, OPT_FRESHCLAM, "By default freshclam will keep the local databases (.cld) uncompressed to\nmake their handling faster. With this option you can enable the compression.\nThe change will take effect with the next database update.", "" },

    { "HTTPProxyServer", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "If you're behind a proxy, please enter its address here.", "your-proxy" },

    { "HTTPProxyPort", NULL, 0, OPT_NUMBER, MATCH_NUMBER, -1, NULL, 0, OPT_FRESHCLAM, "HTTP proxy's port", "8080" },

    { "HTTPProxyUsername", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "A user name for the HTTP proxy authentication.", "username" },

    { "HTTPProxyPassword", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "A password for the HTTP proxy authentication.", "pass" },

    { "HTTPUserAgent", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "If your servers are behind a firewall/proxy which does a User-Agent\nfiltering you can use this option to force the use of a different\nUser-Agent header.", "default" },

    { "NotifyClamd", "daemon-notify", 0, OPT_STRING, NULL, -1, CONFDIR"/clamd.conf", 0, OPT_FRESHCLAM, "Send the RELOAD command to clamd after a successful update.", "yes" },

    { "OnUpdateExecute", "on-update-execute", 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "Run a command after a successful database update.", "command" },

    { "OnErrorExecute", "on-error-execute", 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "Run a command when a database update error occurs.", "command" },

    { "OnOutdatedExecute", "on-outdated-execute", 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "Run a command when freshclam reports an outdated version.\nIn the command string %v will be replaced with the new version number.", "command" },

    /* FIXME: MATCH_IPADDR */
    { "LocalIPAddress", "local-address", 'a', OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "With this option you can provide a client address for the database downlading.\nUseful for multi-homed systems.", "aaa.bbb.ccc.ddd" },

    { "ConnectTimeout", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 30, NULL, 0, OPT_FRESHCLAM, "Timeout in seconds when connecting to database server.", "30" },

    { "ReceiveTimeout", NULL, 0, OPT_NUMBER, MATCH_NUMBER, 30, NULL, 0, OPT_FRESHCLAM, "Timeout in seconds when reading from database server.", "30" },

    { "SubmitDetectionStats", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "", "" },

    { "DetectionStatsCountry", NULL, 0, OPT_STRING, NULL, -1, NULL, 0, OPT_FRESHCLAM, "When enabled freshclam will submit statistics to the ClamAV Project about\nthe latest virus detections in your environment. The ClamAV maintainers\nwill then use this data to determine what types of malware are the most\ndetected in the field and in what geographic area they are.\nThis feature requires LogTime and LogFile to be enabled in clamd.conf.", "/path/to/clamd.conf" },

    /* Deprecated options */

    { "MailMaxRecursion", NULL, 0, OPT_NUMBER, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },
    { "ArchiveMaxScanSize", NULL, 0, OPT_SIZE, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },
    { "ArchiveMaxRecursion", NULL, 0, OPT_NUMBER, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },
    { "ArchiveMaxFiles", NULL, 0, OPT_NUMBER, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },
    { "ArchiveMaxCompressionRatio", NULL, 0, OPT_NUMBER, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },
    { "ArchiveBlockMax", NULL, 0, OPT_BOOL, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },
    { "ArchiveLimitMemoryUsage", NULL, 0, OPT_BOOL, NULL, -1, NULL, 0, OPT_CLAMD | OPT_DEPRECATED, "", "" },

    /* Milter specific options */
/*
    {"ClamdSocket", OPT_QUOTESTR, -1, NULL, 1, OPT_MILTER},
    {"MilterSocket", OPT_QUOTESTR, -1, NULL, 1, OPT_MILTER},
    {"LocalNet", OPT_QUOTESTR, -1, NULL, 1, OPT_MILTER},
    {"OnClean", OPT_QUOTESTR, -1, "Accept", 0, OPT_MILTER},
    {"OnInfected", OPT_QUOTESTR, -1, "Quarantine", 0, OPT_MILTER},
    {"OnFail", OPT_QUOTESTR, -1, "Defer", 0, OPT_MILTER},
    {"AddHeader", OPT_BOOL, 0, NULL, 0, OPT_MILTER},
    {"Chroot", OPT_QUOTESTR, -1, NULL, 0, OPT_MILTER},
    {"Whitelist", OPT_QUOTESTR, -1, NULL, 0, OPT_MILTER},
*/
    /* Deprecated milter options */
/*
    {"ArchiveBlockEncrypted", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"DatabaseDirectory", OPT_QUOTESTR, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"Debug", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"DetectBrokenExecutables", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"LeaveTemporaryFiles", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"LocalSocket", OPT_QUOTESTR, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"MailFollowURLs", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"MaxScanSize", OPT_COMPSIZE, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"MaxFiles", OPT_NUM, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"MaxRecursion", OPT_NUM, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"PhishingSignatures", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"ScanArchive", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"ScanHTML", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"ScanMail", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"ScanOLE2", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"ScanPE", OPT_BOOL, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"StreamMaxLength", OPT_COMPSIZE, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"TCPAddr", OPT_QUOTESTR, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"TCPSocket", OPT_NUM, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
    {"TemporaryDirectory", OPT_QUOTESTR, -1, NULL, 0, OPT_MILTER | OPT_DEPRECATED},
*/
    { NULL, NULL, 0, 0, NULL, 0, NULL, 0, 0, NULL, NULL }
};

const struct optstruct *optget(const struct optstruct *opts, const char *name)
{
    while(opts) {
	if((opts->name && !strcmp(opts->name, name)) || (opts->cmd && !strcmp(opts->cmd, name)))
	    return opts;
	opts = opts->next;
    }
    return NULL;
}

static struct optstruct *optget_i(struct optstruct *opts, const char *name)
{
    while(opts) {
	if((opts->name && !strcmp(opts->name, name)) || (opts->cmd && !strcmp(opts->cmd, name)))
	    return opts;
	opts = opts->next;
    }
    return NULL;
}

/*
static void optprint(const struct optstruct *opts)
{
	const struct optstruct *h;

    printf("\nOPTIONS:\n\n");

    while(opts) {
	printf("OPT_NAME: %s\n", opts->name);
	printf("OPT_CMD: %s\n", opts->cmd);
	printf("OPT_STRARG: %s\n", opts->strarg ? opts->strarg : "NONE");
	printf("OPT_NUMARG: %d\n", opts->numarg);
	h = opts;
	while((h = h->nextarg)) {
	    printf("SUBARG_OPT_STRARG: %s\n", h->strarg ? h->strarg : "NONE");
	    printf("SUBARG_OPT_NUMARG: %d\n", h->numarg);
	}
	printf("----------------\n");
	opts = opts->next;
    }
}
*/

static int optadd(struct optstruct **opts, const char *name, const char *cmd, const char *strarg, int numarg, int multiple, int idx)
{
	struct optstruct *newnode;


    newnode = (struct optstruct *) malloc(sizeof(struct optstruct));

    if(!newnode)
	return -1;

    if(name) {
	newnode->name = strdup(name);
	if(!newnode->name) {
	    free(newnode);
	    return -1;
	}
    } else {
	newnode->name = NULL;
    }

    if(cmd) {
	newnode->cmd = strdup(cmd);
	if(!newnode->cmd) {
	    free(newnode->name);
	    free(newnode);
	    return -1;
	}
    } else {
	newnode->cmd = NULL;
    }

    if(strarg) {
	newnode->strarg = strdup(strarg);
	if(!newnode->strarg) {
	    free(newnode->cmd);
	    free(newnode->name);
	    free(newnode);
	    return -1;
	}
	newnode->enabled = 1;
    } else {
	newnode->strarg = NULL;
	newnode->enabled = 0;
    }
    newnode->numarg = numarg;
    if(numarg && numarg != -1)
	newnode->enabled = 1;
    newnode->nextarg = NULL;
    newnode->next = NULL;
    newnode->active = 0;
    newnode->multiple = multiple;
    newnode->idx = idx;

    newnode->next = *opts;
    *opts = newnode;
    return 0;
}

static int optaddarg(struct optstruct *opts, const char *name, const char *strarg, int numarg)
{
	struct optstruct *pt, *h, *new;


    if(!(pt = optget_i(opts, name))) {
	fprintf(stderr, "ERROR: optaddarg: Unregistered option %s\n", name);
	return -1;
    }

    if(pt->multiple) {
	if(!pt->active) {
	    if(strarg) {
		free(pt->strarg);
		pt->strarg = strdup(strarg);
	 	if(!pt->strarg) {
		    fprintf(stderr, "ERROR: optaddarg: strdup() failed\n");
		    return -1;
		}
	    }
	    pt->numarg = numarg;
	} else {
	    new = (struct optstruct *) calloc(1, sizeof(struct optstruct));
	    if(!new) {
		fprintf(stderr, "ERROR: optaddarg: malloc() failed\n");
		return -1;
	    }
	    if(strarg) {
		new->strarg = strdup(strarg);
	 	if(!new->strarg) {
		    fprintf(stderr, "ERROR: optaddarg: strdup() failed\n");
		    free(new);
		    return -1;
		}
	    }
	    new->numarg = numarg;
	    h = pt;
	    while(h->nextarg)
		h = h->nextarg;
	    h->nextarg = new;
	}
    } else {
	if(pt->active)
	    return 0;

	if(strarg) {
	    free(pt->strarg);
	    pt->strarg = strdup(strarg);
	    if(!pt->strarg) {
		fprintf(stderr, "ERROR: optaddarg: strdup() failed\n");
		return -1;
	    }
	}
	pt->numarg = numarg;
    }

    pt->active = 1;
    if(pt->strarg || (pt->numarg && pt->numarg != -1))
	pt->enabled = 1;

    return 0;
}

void optfree(struct optstruct *opts)
{
    	struct optstruct *h, *a;

    while(opts) {
	a = opts->nextarg;
	while(a) {
	    if(a->strarg) {
		free(a->name);
		free(a->cmd);
		free(a->strarg);
		h = a;
		a = a->nextarg;
		free(h);
	    } else {
		a = a->nextarg;
	    }
	}
	free(opts->name);
	free(opts->cmd);
	free(opts->strarg);
	h = opts;
	opts = opts->next;
	free(h);
    }
    return;
}

struct optstruct *optparse(const char *cfgfile, int argc, char * const *argv, int verbose, int toolmask, struct optstruct *oldopts)
{
	FILE *fs = NULL;
	const struct clam_option *optentry;
	char *pt;
	const char *name = NULL, *arg;
	int i, err = 0, lc = 0, sc = 0, opt_index, line = 0, ret, numarg;
	struct optstruct *opts = NULL, *opt;
	char buff[512];
	struct option longopts[MAXCMDOPTS];
	char shortopts[MAXCMDOPTS];
	regex_t regex;


    if(oldopts)
	opts = oldopts;

    shortopts[sc++] = ':';
    for(i = 0; ; i++) {
	optentry = &clam_options[i];
	if(!optentry->name && !optentry->longopt)
	    break;

	if(optentry->owner & toolmask) {
	    if(!oldopts && optadd(&opts, optentry->name, optentry->longopt, optentry->strarg, optentry->numarg, optentry->multiple, i) < 0) {
		fprintf(stderr, "ERROR: optparse: Can't register new option (not enough memory)\n");
		optfree(opts);
		return NULL;
	    }

	    if(!cfgfile) {
		if(optentry->longopt) {
		    if(lc >= MAXCMDOPTS) {
			fprintf(stderr, "ERROR: optparse: longopts[] is too small\n");
			optfree(opts);
			return NULL;
		    }
		    longopts[lc].name = optentry->longopt;
		    if(optentry->argtype == OPT_BOOL || optentry->strarg)
			longopts[lc].has_arg = 2;
		    else
			longopts[lc].has_arg = 1;
		    longopts[lc].flag = NULL;
		    longopts[lc++].val = optentry->shortopt;
		}
		if(optentry->shortopt) {
		    if(sc + 1 >= MAXCMDOPTS) {
			fprintf(stderr, "ERROR: optparse: shortopts[] is too small\n");
			optfree(opts);
			return NULL;
		    }
		    shortopts[sc++] = optentry->shortopt;
		    /* FIXME: we may need to handle optional args for short
		     * BOOL opts
		     */
		    if(optentry->argtype != OPT_BOOL)
			shortopts[sc++] = ':';
		}
	    }
	}
    }

    if(cfgfile) {
	if((fs = fopen(cfgfile, "rb")) == NULL) {
	    /* don't print error messages here! */
	    optfree(opts);
	    return NULL;
	}
    } else {
	if(MAX(sc, lc) > MAXCMDOPTS) {
	    fprintf(stderr, "ERROR: optparse: (short|long)opts[] is too small\n");
	    optfree(opts);
	    return NULL;
	}
	shortopts[sc] = 0;
	longopts[lc].name = NULL;
	longopts[lc].flag = NULL;
	longopts[lc].has_arg = longopts[lc].val = 0;
    }

    while(1) {

	if(cfgfile) {
	    if(!fgets(buff, sizeof(buff), fs))
		break;

	    line++;
	    if(strlen(buff) <= 2 || buff[0] == '#')
		continue;

	    if(!strncmp("Example", buff, 7)) {
		if(verbose)
		    fprintf(stderr, "ERROR: Please edit the example config file %s\n", cfgfile);
		err = 1;
		break;
	    }

	    if(!(pt = strchr(buff, ' '))) {
		if(verbose)
		    fprintf(stderr, "ERROR: Missing argument for option at line %d\n", line);
		err = 1;
		break;
	    }
	    name = buff;
	    *pt++ = 0;
	    for(i = 0; i < (int) strlen(pt) - 1 && pt[i] == ' '; i++);
	    pt += i;
	    if((i = strlen(pt)) && pt[i - 1] == '\n')
		pt[i-- - 1] = 0;
	    if(!i) {
		if(verbose)
		    fprintf(stderr, "ERROR: Missing argument for option at line %d\n", line);
		err = 1;
		break;
	    }
	    arg = pt;
	    if(*arg == '"') {
		arg++; pt++;
		pt = strrchr(pt, '"');
		if(!pt) {
		    if(verbose)
			fprintf(stderr, "ERROR: Missing closing parenthesis in option %s at line %d\n", name, line);
		    err = 1;
		    break;
		}
		*pt = 0;
		if(!strlen(arg)) {
		    if(verbose)
			fprintf(stderr, "ERROR: Empty argument for option %s at line %d\n", name, line);
		    err = 1;
		    break;
		}
	    }

	} else {
	    opt_index = 0;
	    ret = getopt_long(argc, argv, shortopts, longopts, &opt_index);
	    if(ret == -1)
		break;

	    if(ret == ':') {
		fprintf(stderr, "ERROR: Incomplete option passed (missing argument)\n");
		err = 1;
		break;
	    } else if(!ret || strchr(shortopts, ret)) {
		name = NULL;
		if(ret) {
		    for(i = 0; i < lc; i++) {
			if(ret == longopts[i].val) {
			    name = longopts[i].name;
			    break;
			}
		    }
		} else {
		    name = longopts[opt_index].name;
		}
		if(!name) {
		    fprintf(stderr, "ERROR: optparse: No corresponding long name for option '-%c'\n", (char) ret);
		    err = 1;
		    break;
		}
		optarg ? (arg = optarg) : (arg = NULL);
	    } else {
		fprintf(stderr, "ERROR: Unknown option passed\n");
		err = 1;
		break;
	    }
	}

	if(!name) {
	    fprintf(stderr, "ERROR: Problem parsing options (name == NULL)\n");
	    err = 1;
	    break;
	}

	opt = optget_i(opts, name);
	if(!opt) {
	    if(cfgfile) {
		if(verbose)
		    fprintf(stderr, "ERROR: Parse error at line %d: Unknown option %s\n", line, name);
	    }
	    err = 1;
	    break;
	}
	optentry = &clam_options[opt->idx];

	if(optentry->owner & OPT_DEPRECATED) {
	    if(toolmask & OPT_DEPRECATED) {
		/* FIXME: optadd() -- needed for clamconf */
	    } else {
		if(cfgfile) {
		    if(verbose)
			fprintf(stderr, "WARNING: Ignoring deprecated option %s at line %u\n", opt->name, line);
		} else {
		    if(verbose) {
			if(optentry->shortopt)
			    fprintf(stderr, "WARNING: Ignoring deprecated option --%s (-%c)\n", optentry->longopt, optentry->shortopt);
			else
			    fprintf(stderr, "WARNING: Ignoring deprecated option --%s\n", optentry->longopt);
		    }
		}
		continue;
	    }
	}

	if(!cfgfile && !arg && optentry->argtype == OPT_BOOL) {
	    arg = "yes"; /* default to yes */
	} else if(optentry->regex) {
	    if(cli_regcomp(&regex, optentry->regex, REG_EXTENDED | REG_NOSUB)) {
		fprintf(stderr, "ERROR: optparse: Can't compile regular expression %s for option %s\n", optentry->regex, name);
		err = 1;
		break;
	    }
	    ret = cli_regexec(&regex, arg, 0, NULL, 0);
	    cli_regfree(&regex);
	    if(ret == REG_NOMATCH) {
		if(cfgfile) {
		    fprintf(stderr, "ERROR: Incorrect argument format for option %s\n", name);
		} else {
		    if(optentry->shortopt)
			fprintf(stderr, "ERROR: Incorrect argument format for option --%s (-%c)\n", optentry->longopt, optentry->shortopt);
		    else
			fprintf(stderr, "ERROR: Incorrect argument format for option --%s\n", optentry->longopt);
		}
		err = 1;
		break;
	    }
	}

	numarg = -1;
	switch(optentry->argtype) {
	    case OPT_STRING:
		if(!arg)
		    arg = optentry->strarg;
		if(!cfgfile && !strlen(arg)) {
		    if(optentry->shortopt)
			fprintf(stderr, "ERROR: Option --%s (-%c) requires a non-empty string argument\n", optentry->longopt, optentry->shortopt);
		    else
			fprintf(stderr, "ERROR: Option --%s requires a non-empty string argument\n", optentry->longopt);
		    err = 1;
		    break;
		}
		break;

	    case OPT_NUMBER:
		numarg = atoi(arg);
		arg = NULL;
		break;

	    case OPT_SIZE:
		if(sscanf(arg, "%d", &numarg) != 1) {
		    if(cfgfile) {
			fprintf(stderr, "ERROR: Can't parse numerical argument for option %s\n", name);
		    } else {
			if(optentry->shortopt)
			    fprintf(stderr, "ERROR: Can't parse numerical argument for option --%s (-%c)\n", optentry->longopt, optentry->shortopt);
			else
			    fprintf(stderr, "ERROR: Can't parse numerical argument for option --%s\n", optentry->longopt);
		    }
		    err = 1;
		    break;
		}
		i = strlen(arg) - 1;
		if(arg[i] == 'M' || arg[i] == 'm')
		    numarg *= 1048576;
		else if(arg[i] == 'K' || arg[i] == 'k')
		    numarg *= 1024;
		else
		    numarg = atoi(arg);

		arg = NULL;
		break;

	    case OPT_BOOL:
                if(!strcasecmp(arg, "yes") || !strcmp(arg, "1") || !strcasecmp(arg, "true"))
		    numarg = 1;
		else
		    numarg = 0;

		arg = NULL;
		break;
	}

	if(err)
	    break;

	if(optaddarg(opts, name, arg, numarg) < 0) {
	    if(cfgfile)
		fprintf(stderr, "ERROR: Can't register argument for option %s\n", name);
	    else
		fprintf(stderr, "ERROR: Can't register argument for option --%s\n", optentry->longopt);
	    err = 1;
	    break;
	}
    }

    if(fs)
	fclose(fs);

    if(err) {
	optfree(opts);
	return NULL;
    }

    /* optprint(opts); */

    return opts;
}
