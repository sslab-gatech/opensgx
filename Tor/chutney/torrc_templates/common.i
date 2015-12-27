TestingTorNetwork 1

## Comprehensive Bootstrap Testing Options ##
# These typically launch a working minimal Tor network in 25s-30s
# See authority.tmpl for a partial explanation
#AssumeReachable 0
#Default PathsNeededToBuildCircuits 0.6
#Disable TestingDirAuthVoteExit
#Default V3AuthNIntervalsValid 3

## Rapid Bootstrap Testing Options ##
# These typically launch a working minimal Tor network in 6s-10s
# These parameters make tor networks bootstrap fast,
# but can cause consensus instability and network unreliability
# (Some are also bad for security.)
AssumeReachable 1
PathsNeededToBuildCircuits 0.25
# TestingDirAuthVoteExit *
V3AuthNIntervalsValid 2

## Always On Testing Options ##
# We enable TestingDirAuthVoteGuard to avoid Guard stability requirements
TestingDirAuthVoteGuard *
# We set TestingMinExitFlagThreshold to 0 to avoid Exit bandwidth requirements
TestingMinExitFlagThreshold 0

DataDirectory $dir
RunAsDaemon 1
ConnLimit $connlimit
Nickname $nick
ShutdownWaitLength 0
PidFile ${dir}/pid
Log notice file ${dir}/notice.log
Log info file ${dir}/info.log
# Turn this off to save space
#Log debug file ${dir}/debug.log
ProtocolWarnings 1
SafeLogging 0
DisableDebuggerAttachment 0
${authorities}
