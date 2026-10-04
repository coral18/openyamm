#include "doctest/doctest.h"

#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/indoor/IndoorMovementController.h"
#include "game/outdoor/OutdoorMovementController.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

using namespace OpenYAMM::Game;

namespace
{
constexpr float StepSeconds = 1.0f / 128.0f;
constexpr float ActorRadius = 40.0f;
constexpr float ActorHeight = 128.0f;

IndoorMapData makeCrowdingRoom(bool doorway)
{
    IndoorMapData map = {};
    map.vertices = {
        {-1000, -1000, 0}, {1000, -1000, 0}, {1000, 1000, 0}, {-1000, 1000, 0},
        {-1000, -1000, 500}, {1000, -1000, 500}, {1000, 1000, 500}, {-1000, 1000, 500}
    };
    const std::array<std::vector<uint16_t>, 6> faceVertices = {{
        {0, 1, 2, 3}, {7, 6, 5, 4}, {0, 4, 5, 1}, {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}
    }};
    for (size_t index = 0; index < faceVertices.size(); ++index)
    {
        IndoorFace face = {};
        face.vertexIndices = faceVertices[index];
        face.facetType = index == 0 ? 3 : index == 1 ? 5 : 1;
        face.roomNumber = 1;
        map.faces.push_back(face);
    }
    map.sectors.resize(2);
    IndoorSector &room = map.sectors[1];
    room.minX = room.minY = -1000;
    room.maxX = room.maxY = 1000;
    room.maxZ = 500;
    room.floorFaceIds = {0};
    room.ceilingFaceIds = {1};
    room.wallFaceIds = {2, 3, 4, 5};
    room.faceIds = {0, 1, 2, 3, 4, 5};
    if (doorway)
    {
        for (const std::array<int, 2> span : {std::array<int, 2>{-1000, -52}, {52, 1000}})
        {
            const uint16_t offset = map.vertices.size();
            map.vertices.insert(map.vertices.end(), {
                {0, span[0], 0}, {0, span[1], 0}, {0, span[1], 500}, {0, span[0], 500}});
            IndoorFace wall = {};
            wall.vertexIndices = {offset, uint16_t(offset + 3), uint16_t(offset + 2), uint16_t(offset + 1)};
            wall.facetType = 1;
            wall.roomNumber = 1;
            room.wallFaceIds.push_back(map.faces.size());
            room.faceIds.push_back(map.faces.size());
            map.faces.push_back(wall);
        }
    }
    return map;
}

OutdoorMapData makeCrowdingGround(bool doorway)
{
    OutdoorMapData map = {};
    map.heightMap.resize(OutdoorMapData::TerrainWidth * OutdoorMapData::TerrainHeight, 0);
    map.attributeMap.resize(map.heightMap.size(), 0);
    if (doorway)
    {
        OutdoorBModel walls = {};
        const IndoorMapData room = makeCrowdingRoom(true);
        for (size_t index = 8; index < room.vertices.size(); ++index)
        {
            const IndoorVertex &vertex = room.vertices[index];
            walls.vertices.push_back({vertex.x, vertex.y, vertex.z});
        }
        for (const std::vector<uint16_t> vertices : {std::vector<uint16_t>{0, 3, 2, 1}, {4, 7, 6, 5}})
        {
            OutdoorBModelFace wall = {};
            wall.vertexIndices = vertices;
            wall.polygonType = 1;
            walls.faces.push_back(wall);
            std::reverse(wall.vertexIndices.begin(), wall.vertexIndices.end());
            walls.faces.push_back(wall);
        }
        map.bmodels.push_back(std::move(walls));
    }
    return map;
}

struct CrowdMoveResult
{
    GameplayWorldPoint position = {};
    std::vector<size_t> contacts;
};

class CrowdMovementFixture
{
public:
    explicit CrowdMovementFixture(bool indoor, bool doorway = false)
        : m_indoor(indoor), m_room(makeCrowdingRoom(doorway)), m_ground(makeCrowdingGround(doorway)),
          m_indoorController(m_room, nullptr, nullptr),
          m_outdoorController(m_ground, std::nullopt, std::nullopt, std::nullopt, std::nullopt)
    {
    }

    CrowdMoveResult move(
        const std::vector<GameplayWorldPoint> &positions, size_t actorIndex, float velocityX, float velocityY)
    {
        CrowdMoveResult result = {};
        const GameplayWorldPoint &position = positions[actorIndex];
        if (m_indoor)
        {
            std::vector<IndoorActorCollision> colliders;
            for (size_t index = 0; index < positions.size(); ++index)
            {
                const GameplayWorldPoint &peer = positions[index];
                colliders.push_back({index, 1, peer.x, peer.y, peer.z, ActorRadius, ActorHeight, true});
            }
            m_indoorController.setActorColliders(colliders);
            const IndoorBodyDimensions body{ActorRadius, ActorHeight};
            const IndoorMoveState start = m_indoorController.initializeStateFromEyePosition(
                position.x, position.y, position.z + body.height, body);
            const IndoorMoveState end = m_indoorController.resolveMove(
                start, body, velocityX, velocityY, false, StepSeconds, &result.contacts, actorIndex,
                false, nullptr, false, false, 420.0f, 1.0f, false, false, false, true, true);
            result.position = {end.x, end.y, end.footZ};
        }
        else
        {
            std::vector<OutdoorActorCollision> colliders;
            for (size_t index = 0; index < positions.size(); ++index)
            {
                OutdoorActorCollision collider = {};
                collider.source = OutdoorActorCollisionSource::MapDelta;
                collider.sourceIndex = index;
                collider.worldX = positions[index].x;
                collider.worldY = positions[index].y;
                collider.worldZ = positions[index].z;
                collider.radius = ActorRadius;
                collider.height = ActorHeight;
                colliders.push_back(collider);
            }
            m_outdoorController.setActorColliders(colliders);
            const OutdoorMoveState start = m_outdoorController.initializeActorStateForBodyPreservingZ(
                position.x, position.y, position.z, ActorRadius);
            const OutdoorMoveState end = m_outdoorController.resolveOutdoorActorMove(
                start, {ActorRadius, ActorHeight}, velocityX, velocityY, 0, false, StepSeconds,
                &result.contacts, OutdoorIgnoredActorCollider{OutdoorActorCollisionSource::MapDelta, actorIndex});
            result.position = {end.x, end.y, end.footZ};
        }
        return result;
    }

private:
    bool m_indoor;
    IndoorMapData m_room;
    OutdoorMapData m_ground;
    IndoorMovementController m_indoorController;
    OutdoorMovementController m_outdoorController;
};
}

TEST_CASE("actor crowding keeps touching monsters solid and lets them separate")
{
    for (bool indoor : {false, true})
    {
        CAPTURE(indoor);
        CrowdMovementFixture world(indoor);
        for (float separation : {80.0f, 79.5f, 16.0f})
        {
            CAPTURE(separation);
            const std::vector<GameplayWorldPoint> actors = {{0.375f, 0, 1}, {0.375f + separation, 0, 1}};
            const CrowdMoveResult inward = world.move(actors, 0, 200, 0);
            CHECK(inward.position.x == doctest::Approx(actors[0].x));
            CHECK_FALSE(inward.contacts.empty());

            const CrowdMoveResult outward = world.move(actors, 0, -200, 0);
            CHECK(outward.position.x < actors[0].x - 1.0f);
            CHECK(outward.contacts.empty());

            const CrowdMoveResult tangent = world.move(actors, 0, 0, 200);
            CHECK(tangent.position.y > 1.0f);
            CHECK(tangent.contacts.empty());
        }
    }
}

TEST_CASE("actor crowding preserves separation over repeated head-on movement")
{
    for (bool indoor : {false, true})
    {
        CAPTURE(indoor);
        CrowdMovementFixture world(indoor);
        for (float speed : {128.0f, 137.0f, 200.0f, 251.0f})
        {
            CAPTURE(speed);
            std::vector<GameplayWorldPoint> actors = {{-100.375f, 0, 1}, {100.125f, 0, 1}};
            size_t contacts = 0;
            float minimumSeparation = 200.5f;
            for (int tick = 0; tick < 128; ++tick)
            {
                for (size_t index = 0; index < actors.size(); ++index)
                {
                    const CrowdMoveResult move = world.move(actors, index, index == 0 ? speed : -speed, 0);
                    actors[index] = move.position;
                    contacts += move.contacts.size();
                    minimumSeparation = std::min(minimumSeparation, actors[1].x - actors[0].x);
                }
            }
            CHECK(minimumSeparation >= 2.0f * ActorRadius - 0.01f);
            CHECK(actors[1].x - actors[0].x == doctest::Approx(2.0f * ActorRadius).epsilon(0.001f));
            CHECK(contacts > 0);
        }
    }
}

TEST_CASE("actor crowding slides around a touching monster without penetrating it")
{
    for (bool indoor : {false, true})
    {
        CAPTURE(indoor);
        CrowdMovementFixture world(indoor);
        std::vector<GameplayWorldPoint> actors = {{-80.0f, 0, 1}, {0, 0, 1}};
        float minimumSeparation = 80.0f;
        for (int tick = 0; tick < 128; ++tick)
        {
            const CrowdMoveResult move = world.move(actors, 0, 140, 140);
            actors[0] = move.position;
            minimumSeparation = std::min(minimumSeparation, std::hypot(actors[0].x, actors[0].y));
        }
        CHECK(minimumSeparation >= 2.0f * ActorRadius - 0.01f);
        CHECK(actors[0].x > 0.0f);
        CHECK(actors[0].y > 80.0f);
    }
}

TEST_CASE("actor crowding allows coincident monsters to escape and ignores vertically separate monsters")
{
    for (bool indoor : {false, true})
    {
        CAPTURE(indoor);
        CrowdMovementFixture world(indoor);
        for (float peerZ : {1.0f, 300.0f})
        {
            const std::vector<GameplayWorldPoint> actors = {{0, 0, 1}, {0, 0, peerZ}};
            const CrowdMoveResult move = world.move(actors, 0, 200, 0);
            CHECK(move.position.x > 1.0f);
            CHECK(move.contacts.empty());
        }
    }
}

TEST_CASE("actor crowding respects doorway edges while sliding past other monsters")
{
    for (bool indoor : {false, true})
    {
        CAPTURE(indoor);
        CrowdMovementFixture world(indoor, true);
        std::vector<GameplayWorldPoint> actors = {{-100.0f, -45.0f, 1}, {-100.0f, 45.0f, 1}};
        float minimumSeparation = 90.0f;
        float minimumWallDistance = 100.0f;
        for (int tick = 0; tick < 512; ++tick)
        {
            for (size_t index = 0; index < actors.size(); ++index)
            {
                const GameplayWorldPoint position = actors[index];
                const float dx = 700.0f - position.x;
                const float dy = -position.y;
                const float distance = std::hypot(dx, dy);
                actors[index] = world.move(actors, index, dx / distance * 160.0f, dy / distance * 160.0f).position;
                const float edgeY = std::max(0.0f, 52.0f - std::abs(actors[index].y));
                minimumWallDistance = std::min(minimumWallDistance, std::hypot(actors[index].x, edgeY));
                minimumSeparation = std::min(minimumSeparation,
                    std::hypot(actors[0].x - actors[1].x, actors[0].y - actors[1].y));
            }
        }
        CHECK(minimumSeparation >= 2.0f * ActorRadius - 0.01f);
        CHECK(minimumWallDistance >= ActorRadius - 0.01f);
    }
}

TEST_CASE("actor crowding cannot cut doorway edges when already inside the wall plane radius")
{
    for (bool indoor : {false, true})
    {
        CAPTURE(indoor);
        CrowdMovementFixture world(indoor, true);
        for (float velocityX : {-200.0f, 0.0f, 200.0f})
        {
            CAPTURE(velocityX);
            std::vector<GameplayWorldPoint> actors = {{-8.7f, 12.5f, 1}};
            float minimumWallDistance = 100.0f;
            for (int tick = 0; tick < 16; ++tick)
            {
                actors[0] = world.move(actors, 0, velocityX, 200.0f).position;
                const float edgeY = std::max(0.0f, 52.0f - std::abs(actors[0].y));
                minimumWallDistance = std::min(minimumWallDistance, std::hypot(actors[0].x, edgeY));
            }
            CHECK(minimumWallDistance >= ActorRadius - 0.01f);
        }
    }
}
