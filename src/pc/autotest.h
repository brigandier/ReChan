#pragma once
int AutoTest_VerifyLocalization(const char* outputDirectory);
#if defined(RECHAN_VISUAL_TESTS)
int AutoTest_VerifyEngineIntegration(const char* outputDirectory);
int AutoTest_RenderMovieSubtitles(const char* movieDirectory, const char* outputDirectory);
#endif
#include "gen/common.h"

enum class GameState : s32;

// rechan --autotest <levelIndex> [--level-id N] [--petal N] [--frames N] [--fps N]
//        [--hold up,cross,...] [--screenshot out.png]
//        [--warp-generator N] [--kill-interval N] [--trace-spawns]
//        [--benchmark-flow finish|restart|hub|petal]
//        [--benchmark-score-percent 0..100]
// Exit code: 0 finished, 10 data missing, 11 gameplay timeout, 12 capture failed, 13 flow timeout, 14 invalid flow.
void AutoTest_ParseArgs(int argc, char** argv);
bool AutoTest_IsEnabled();

s32 AutoTest_GetLevelIndex();
s32 AutoTest_GetPetalIndex();
bool AutoTest_TraceSpawns();
s32 AutoTest_GetFps();
f32 AutoTest_GetFixedDeltaTime();
u32 AutoTest_GetHeldButtons();

void AutoTest_Fail(int exitCode, const char* reason);

// Returns false when the run is over.
bool AutoTest_Step(GameState state);
int AutoTest_GetExitCode();
