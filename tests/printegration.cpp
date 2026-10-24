#include "gen/common.h"
#include "ai/activezn.h"
#include "ai/behaviour.h"
#include "ai/boss.h"
#include "ai/door.h"
#include "ai/generator.h"
#include "ai/player.h"
#include "extra/modloader.h"
#include "fe/hud.h"
#include "gen/ai.h"
#include "gen/animstruct.h"
#include "gen/camera.h"
#include "gen/database.h"
#include "gen/display.h"
#include "gen/game.h"
#include "gen/model.h"
#include "gen/time.h"
#include "p3d/flip.h"
#include "p3d/hash.h"
#include "snd/rsdworld.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

static void Check(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}

static void TestEncounters() {
    Game game;
    AI ai;
    g_ai = &ai;
    DBVolume volume;
    volume.SetName("encounter", 1);
    ActiveZone first(&volume), second(&volume);
    second.SetName("other", 1);
    ai.activeZoneList.AddNodeTail(&first);
    ai.activeZoneList.AddNodeTail(&second);
    LVector pos{};
    {
        Humanoid one(&pos, 1), two(&pos, 1);
        one.activeZone = two.activeZone = &first;
        one.field452 = two.field452 = 1;
        one.RegisterEncounterPopulation();
        one.RegisterEncounterPopulation();
        two.RegisterEncounterPopulation();
        Check(first.GetEncounterHumanoidsRemaining() == 2, "duplicate humanoid registration");
        first.ResolveEncounterHumanoid(&one);
        first.ResolveEncounterHumanoid(&one);
        Check(first.GetEncounterHumanoidsRemaining() == 1, "resolution consumed another humanoid");
        one.ResolveEncounterPopulation();
        one.Reset();
        Check(first.GetEncounterHumanoidsRemaining() == 2, "reset did not restore encounter");
        one.activeZone = &second;
        one.RegisterEncounterPopulation();
        Check(first.GetEncounterHumanoidsRemaining() == 1 && second.GetEncounterHumanoidsRemaining() == 1,
            "zone reassignment left an unresolved member");
        one.Killed();
        one.Killed();
        Check(!second.HasUnresolvedEncounter(), "repeated death underflow");
        first.AddHumanoidToOverlordMembers(&two);
        first.AddHumanoidToOverlordMembers(&two);
        Check(first.memberCount == 1, "duplicate overlord member");
        first.RemoveHumanoidFromOverlordMembers(&two);
        first.RemoveHumanoidFromOverlordMembers(&two);
        Check(!first.memberCount, "overlord removal underflow");
        EnemyGenerator generator(&pos, 1), other(&pos, 1);
        first.RegisterPendingEnemySpawns(&generator, 2);
        first.RegisterPendingEnemySpawns(&other, 1);
        first.RemovePendingEnemySpawns(&generator, 99);
        first.ResolvePendingEnemySpawn(&generator);
        Check(first.GetPendingEnemySpawns() == 1, "generator consumed another wave");
        first.ResolvePendingEnemySpawn(&other);
        Check(!first.GetPendingEnemySpawns(), "pending waves not exhausted");
        Door door(&pos, 1);
        door.killThingsCRC = (s32)first.nameCRC;
        door.Reset();
        for (int i = 0; i < 10; ++i) door.DeathCheck();
        Check(door.doorState == 0, "guarded door opened before authored enemy died");
        two.Killed();
        for (int i = 0; i < 3; ++i) door.DeathCheck();
        Check(door.doorState == 0, "door skipped original unlock delay");
        door.DeathCheck();
        Check(door.doorState == 1, "door remains locked after encounter");
    }
    Check(!first.HasUnresolvedEncounter() && !second.HasUnresolvedEncounter(), "destruction leaked encounter counts");
    ai.activeZoneList.RemNode(&first);
    ai.activeZoneList.RemNode(&second);
    g_ai = nullptr;
    std::puts("PASS encounters: identity, duplicate registration, reset, zone transfer, death, waves, door delay and teardown");
}

static void TestAnimation() {
    STreeData tree;
    tree.numJoints = tree.numMapEntries = 1;
    tree.joints = new STreeJoint[1]{};
    tree.jointOrderMap = new u32[1]{};
    std::array<u32, 8> raw{};
    raw[2] = 2; raw[3] = 5; raw[4] = 6;
    ((u8*)raw.data())[20] = 0;
    ((u8*)raw.data())[21] = 1;
    const auto packed = [](u16 x, u16 y, u16 z) { return ((u32)x >> 5) | (((u32)y >> 5) << 11) | (((u32)z >> 6) << 22); };
    raw[6] = packed(0, 0, 0x4000);
    raw[7] = packed(0x8000, 0x8000, 0x4000);
    TransformAnim clip;
    clip.numFrames = 2;
    clip.numRotChannels = 1;
    clip.rotChannels = new TransformAnim::Channel[1]{{0, KEY_JOINT_3DOF_ANGLE,
        (const u8*)raw.data(), (const u8*)raw.data(), sizeof(raw)}};
    TransformFlip flip;
    flip.Attach(&tree, &clip);
    for (int fraction : {0, 16384, 32768, 49152}) {
        flip.SetFrameReal(fraction);
        flip.UpdateJoints();
        Check(std::abs((int)tree.joints[0].rotationX) < 64 && std::abs((int)tree.joints[0].rotationY) < 64,
            "Euler branch interpolation introduced a half-turn");
    }
    AnimStructure anim(0, nullptr, ANIM_STOP);
    anim.flip = &flip;
    anim.loopTypeField = ANIM_BLEND2;
    anim.field56 = FIX16_ONE;
    anim.endFrame = 3 * FIX16_ONE;
    anim.ApplyFlipPose(FIX16_ONE);
    Check(flip.frameReal == FIX16_ONE, "Blend2 sampled clock instead of destination");
    anim.ApplyFlipPose(anim.endFrame);
    Check(flip.frameReal == FIX16_ONE, "full-weight Blend2 changed sample");
    anim.flip = nullptr;
    std::puts("PASS animation: fractional Euler branch continuity and Blend2 destination at partial/full weight");
}

static void TestBossAndSpatialAudio() {
    LVector pos{1000, 0, 1000};
    Player player(&pos);
    Grontar boss(&pos);
    boss.commandBits = 1 << GA_DIVE_ROLL;
    boss.SetActionState(AS_STRAFE, 0);
    Check(boss.stateDispatch == SD_GRONTAR_COMBO, "Barney combo request lost its dispatch");
    boss.ProcessAction();
    boss.commandBits = 0;
    boss.SetActionState(AS_STRAFE, 0);
    Check(boss.stateDispatch == SD_STRAFE, "ordinary strafe routed to combo");
    Behaviour decision(&boss, 0, 0);
    Behaviour::GrontarDMS(&decision);
    Check(!boss.activeZone, "missing-zone fallback mutated the encounter");
    Display display;
    display.InternalOpen();
    Camera* camera = display.GetCamera();
    camera->SetPosition(90000, 0, 90000);
    Check(camera->GetCamAngleY() == 0, "camera reset yaw");
    u16 left = 0, right = 0;
    rsdWorld::ComputeObjectVolumes(10000, &player.pos, left, right, 0);
    Check(left == 10000 && right == 10000, "nearby dialogue attenuated by distant camera");
    LVector source{player.pos.x + 3000, 0, player.pos.z};
    rsdWorld::ComputeObjectVolumes(10000, &source, left, right, 0);
    const u16 originalLeft = left, originalRight = right;
    Check(left != right && left && right, "spatial stereo separation missing");
    source.x = player.pos.x - 3000;
    rsdWorld::ComputeObjectVolumes(10000, &source, left, right, 0);
    Check(left == originalRight && right == originalLeft, "opposite source did not reverse stereo separation");
    source.x += 30000;
    rsdWorld::ComputeObjectVolumes(10000, &source, left, right, 0);
    Check(!left && !right, "distant sound did not attenuate");
    std::puts("PASS Barney combo/strafe routing, missing zone, player-centered audio, opposite-source stereo and distance attenuation");
}

static void WriteWave(const std::filesystem::path& path, s16 value, bool malformed = false) {
    std::filesystem::create_directories(path.parent_path());
    const u32 wave[] = {0x46464952, 38, 0x45564157, 0x20746d66, 16, 0x00010001,
        22050, 44100, 0x00100002, 0x61746164, malformed ? 0xfffffff0u : 2};
    std::ofstream file(path, std::ios::binary);
    file.write((const char*)wave, sizeof(wave));
    file.write((const char*)&value, sizeof(value));
}

static void TestSoundOverrides(const std::filesystem::path& output) {
    const auto original = std::filesystem::current_path();
    const auto fixture = output / "mod-fixture";
    Check(!std::filesystem::exists(fixture), "fixture directory already exists; use a fresh output path");
    const auto sounds = fixture / "~mods/test/sounds/dialog";
    WriteWave(sounds / "aq.wav", 123);
    WriteWave(sounds / "ba.wav", 456);
    WriteWave(sounds / "broken.wav", 789, true);
    Check(p3dHash("dialog/aq") == p3dHash("dialog/ba"), "WAV fixture must collide");
    std::filesystem::current_path(fixture);
    auto& loader = ModLoader::Instance();
    loader.Init();
    std::vector<s16> pcm;
    u32 rate = 0, channels = 0;
    Check(loader.GetSoundOverridePCM("DIALOG", "AQ", pcm, rate, channels) && pcm.size() == 1 && pcm[0] == 123,
        "first colliding WAV key lost");
    Check(loader.GetSoundOverridePCM("dialog", "ba", pcm, rate, channels) && pcm[0] == 456,
        "second colliding WAV key lost");
    Check(rate == 22050 && channels == 1, "WAV format changed");
    Check(!loader.GetSoundOverridePCM("dialog", "broken", pcm, rate, channels), "malformed WAV size accepted");
    Check(!loader.FindSoundOverridePath("dialog", "missing"), "missing WAV resolved unexpectedly");
    loader.Reload();
    Check(loader.GetSoundOverridePCM("dialog", "aq", pcm, rate, channels) && pcm[0] == 123, "WAV reload lost exact keys");
    loader.Shutdown();
    std::filesystem::current_path(original);
    std::puts("PASS WAV overrides: colliding hashes, case normalization, PCM, missing file, oversized chunk and reload");
}

int AutoTest_VerifyEngineIntegration(const char* outputDirectory) {
    try {
        const auto output = std::filesystem::absolute(outputDirectory);
        std::filesystem::create_directories(output);
        TestEncounters();
        TestAnimation();
        TestBossAndSpatialAudio();
        TestSoundOverrides(output);
        HUD hud;
        Check(hud.dragonShowState == (CUSTOM_MENU ? 1 : 0), "HUD initial toggle state");
        hud.ToggleShowAll();
        Check(!hud.dragonShowState, "HUD toggle off");
        hud.ToggleShowAll();
        Check(hud.dragonShowState, "HUD toggle on");
        std::puts("PASS custom HUD initial status-display state");
        return 0;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL engine integration: %s\n", error.what());
        return 40;
    }
}
