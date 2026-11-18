#include <tchips/client/asset_manager.hpp>
#include <tchips/client/camera2d.hpp>
#include <tchips/client/movement_system.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <type_traits>

namespace {
using namespace tchips::client;
using namespace tchips::math;
WorldMap map_from(const std::string& text) {
    std::istringstream input{text};auto result=WorldMap::parse(input);
    if(!result) throw std::runtime_error(result.error());
    return std::move(*result);
}
WorldMap empty_map() { return map_from("TCHIPS_MAP 1 size 3000 3000 tile 64 spawn 200 200 obstacles 0"); }
WorldPosition run_frames(int frames,double duration,WorldVector input) {
    auto map=empty_map();entt::registry registry;const auto player=registry.create();
    registry.emplace<CharacterTransform>(player,map.spawn(),map.spawn(),0.0);registry.emplace<CharacterBody>(player);
    MovementSystem movement;
    for(int i=0;i<frames;++i) movement.advance(registry,map,input,duration/frames);
    return registry.get<CharacterTransform>(player).position;
}
}
TEST(Movement, DiagonalHasNoSpeedAdvantage) {
    const auto straight=run_frames(60,1,{1,0}),diagonal=run_frames(60,1,{1,1});
    EXPECT_NEAR(straight.x-200,240,1e-8);
    EXPECT_NEAR(std::hypot(diagonal.x-200,diagonal.y-200),240,1e-8);
}
TEST(Movement, SameDistanceAt30_60_144And240FramesPerSecond) {
    for(const auto fps:{30,60,144,240}) {
        const auto result=run_frames(fps*2,2,{1,0});EXPECT_NEAR(result.x,680,1e-8)<<fps;EXPECT_DOUBLE_EQ(result.y,200);
    }
}
TEST(Movement, AimUsesWorldCoordinates) {
    CharacterTransform transform{{100,100},{100,100},0};MovementSystem::aim(transform,{100,200});
    EXPECT_NEAR(transform.angle,std::acos(-1.0)/2,1e-12);
    MovementSystem::aim(transform,{100,100});EXPECT_NEAR(transform.angle,std::acos(-1.0)/2,1e-12);
}
TEST(Movement, BacklogIsBoundedAndInvalidFrameTimeIgnored) {
    auto map=empty_map();entt::registry registry;const auto player=registry.create();registry.emplace<CharacterTransform>(player,map.spawn(),map.spawn(),0.0);registry.emplace<CharacterBody>(player);
    MovementSystem movement;EXPECT_EQ(movement.advance(registry,map,{1,0},10),8);
    EXPECT_LE(registry.get<CharacterTransform>(player).position.x,232.000001);
    EXPECT_EQ(movement.advance(registry,map,{1,0},-1),0);
    EXPECT_EQ(movement.advance(registry,map,{1,0},std::numeric_limits<double>::quiet_NaN()),0);
    EXPECT_GE(movement.alpha(),0);EXPECT_LT(movement.alpha(),1);
}
TEST(Collision, SweptMovementCannotCrossThinObstacle) {
    auto map=map_from("TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 200 50 1 500");
    const auto result=CollisionSystem::move(map,{100,100},{700,0},12);
    EXPECT_DOUBLE_EQ(result.x,188);EXPECT_DOUBLE_EQ(result.y,100);EXPECT_TRUE(CollisionSystem::fits(map,result,12));
    EXPECT_FALSE(CollisionSystem::fits(map,{195,100},12));
}
TEST(Collision, SlidesAlongWallAndRespectsWorldBounds) {
    auto map=map_from("TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 200 50 100 500");
    const auto result=CollisionSystem::move(map,{188,100},{20,30},12);EXPECT_DOUBLE_EQ(result.x,188);EXPECT_DOUBLE_EQ(result.y,130);
    const auto boundary=CollisionSystem::move(map,{100,100},{-500,-500},12);EXPECT_DOUBLE_EQ(boundary.x,12);EXPECT_DOUBLE_EQ(boundary.y,12);
    const auto reverse=CollisionSystem::move(map,{400,100},{-700,0},12);EXPECT_DOUBLE_EQ(reverse.x,312);
}
TEST(Collision, VerticalSweepAndCornerContactAreValid) {
    auto map=map_from("TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 200 200 100 100");
    EXPECT_DOUBLE_EQ(CollisionSystem::move(map,{250,100},{0,600},12).y,188);
    EXPECT_DOUBLE_EQ(CollisionSystem::move(map,{250,400},{0,-600},12).y,312);
    EXPECT_TRUE(CollisionSystem::fits(map,{188,188},12));
}
TEST(Camera, RoundTripAndViewportCenter) {
    static_assert(!std::is_convertible_v<WorldPosition,ScreenPosition>);
    Camera2D camera;camera.set_viewport({800,600});camera.follow({1000,900},3000,3000);
    const auto center=camera.world_to_screen({1000,900});EXPECT_DOUBLE_EQ(center.x,400);EXPECT_DOUBLE_EQ(center.y,300);
    const auto screen=camera.world_to_screen({1250,800});const auto restored=camera.screen_to_world(screen);
    EXPECT_DOUBLE_EQ(restored.x,1250);EXPECT_DOUBLE_EQ(restored.y,800);
    camera.set_viewport({1024,768});const auto after=camera.screen_to_world(camera.world_to_screen({1250,800}));EXPECT_DOUBLE_EQ(after.x,1250);EXPECT_DOUBLE_EQ(after.y,800);
}
TEST(Camera, SmallWorldIsCenteredAndLargeWorldIsClamped) {
    Camera2D camera;camera.set_viewport({800,600});camera.follow({0,0},2000,1000);EXPECT_DOUBLE_EQ(camera.center().x,400);EXPECT_DOUBLE_EQ(camera.center().y,300);
    camera.follow({2000,1000},2000,1000);EXPECT_DOUBLE_EQ(camera.center().x,1600);EXPECT_DOUBLE_EQ(camera.center().y,700);
    camera.follow({0,0},200,100);EXPECT_DOUBLE_EQ(camera.center().x,100);EXPECT_DOUBLE_EQ(camera.center().y,50);
}
TEST(WorldMap, LoadsActualMapAndSpriteFromFiles) {
    const std::filesystem::path root{TCHIPS_TEST_CONTENT_DIR};auto map=WorldMap::load(root/"maps/prototype.map");ASSERT_TRUE(map)<<map.error();
    EXPECT_DOUBLE_EQ(map->width(),2560);EXPECT_EQ(map->obstacles().size(),10);EXPECT_TRUE(CollisionSystem::fits(*map,map->spawn(),12));
    AssetManager assets{root};auto sprite=assets.load_character();ASSERT_TRUE(sprite)<<sprite.error();EXPECT_EQ(sprite->width,12);EXPECT_FALSE(sprite->pixels.empty());
}
TEST(WorldMap, RejectsInvalidOrTruncatedDefinitions) {
    for(const auto text:{
        "TCHIPS_MAP 2 size 1000 1000 tile 64 spawn 100 100 obstacles 0",
        "TCHIPS_MAP 1 size -1 1000 tile 64 spawn 100 100 obstacles 0",
        "TCHIPS_MAP 1 size 100001 1000 tile 64 spawn 100 100 obstacles 0",
        "TCHIPS_MAP 1 size 1000 1000 tile 0 spawn 100 100 obstacles 0",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles -1",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 513",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 200 200 -1 20",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 900 900 200 200",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 90 90 20 20",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 1 200 200",
        "TCHIPS_MAP 1 size 1000 1000 tile 64 spawn 100 100 obstacles 0 trailing",
        "TCHIPS_MAP 1 size nan 1000 tile 64 spawn 100 100 obstacles 0"}) {
        std::istringstream input{text};EXPECT_FALSE(WorldMap::parse(input))<<text;
    }
    EXPECT_FALSE(WorldMap::load(std::filesystem::path{TCHIPS_TEST_CONTENT_DIR}/"missing.map"));
}
TEST(Assets, RejectsMalformedSprites) {
    for(const auto text:{"TCHIPS_SPRITE 2 1 1 a","TCHIPS_SPRITE 1 0 1 a","TCHIPS_SPRITE 1 33 1 a","TCHIPS_SPRITE 1 2 1 a","TCHIPS_SPRITE 1 1 1 z","TCHIPS_SPRITE 1 1 1 .","TCHIPS_SPRITE 1 1 1 a trailing"}) {
        std::istringstream input{text};EXPECT_FALSE(AssetManager::parse_sprite(input))<<text;
    }
}
