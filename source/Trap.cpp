/*
 *  Copyright (C) 2011-2014  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Trap.h"

#include "ODServer.h"
#include "ServerNotification.h"
#include "Tile.h"
#include "RenderRequest.h"
#include "RenderManager.h"
#include "Seat.h"
#include "GameMap.h"
#include "TrapCannon.h"
#include "TrapBoulder.h"
#include "Random.h"
#include "SoundEffectsHelper.h"
#include "Player.h"
#include "LogManager.h"

const double Trap::mDefaultTileHP = 10.0;

Trap::Trap(GameMap* gameMap) :
    Building(gameMap),
    mReloadTime(0),
    mReloadTimeCounter(0),
    mMinDamage(0.0),
    mMaxDamage(0.0),
    mType(nullTrapType)
{
    setObjectType(GameEntity::trap);
}

void Trap::createMeshLocal()
{
    Building::createMeshLocal();

    if(getGameMap()->isServerGameMap())
        return;

    std::vector<Tile*> coveredTiles = getCoveredTiles();
    for (unsigned int i = 0, nb = coveredTiles.size(); i < nb; ++i)
    {
        RenderRequest* request = new RenderRequest;
        request->type = RenderRequest::createTrap;
        request->p    = static_cast<void*>(this);
        request->p2   = coveredTiles[i];
        RenderManager::queueRenderRequest(request);
    }
}

void Trap::destroyMeshLocal()
{
    Building::destroyMeshLocal();

    if(getGameMap()->isServerGameMap())
        return;

    std::vector<Tile*> coveredTiles = getCoveredTiles();
    for (unsigned int i = 0, nb = coveredTiles.size(); i < nb; ++i)
    {
        RenderRequest *request = new RenderRequest;
        request->type = RenderRequest::destroyTrap;
        request->p    = static_cast<void*>(this);
        request->p2   = coveredTiles[i];
        RenderManager::queueRenderRequest(request);
    }
}

void Trap::deleteYourselfLocal()
{
    Building::deleteYourselfLocal();
    if(getGameMap()->isServerGameMap())
        return;

    RenderRequest* request = new RenderRequest;
    request->type = RenderRequest::deleteTrap;
    request->p = static_cast<void*>(this);
    RenderManager::queueRenderRequest(request);
}

Trap* Trap::createTrap(GameMap* gameMap, TrapType nType, const std::vector<Tile*> &nCoveredTiles,
    Seat *nControllingSeat, const std::string& nameToUse, void* params)
{
    Trap *tempTrap = NULL;

    switch (nType)
    {
        case nullTrapType:
            tempTrap = NULL;
            break;
        case cannon:
            tempTrap = new TrapCannon(gameMap);
            break;
        case boulder:
            if(params != NULL)
            {
                int* p = (int*)params;
                tempTrap = new TrapBoulder(gameMap, p[0], p[1]);
            }
            break;
    }

    if (tempTrap == NULL)
    {
        std::cerr
        << "\n\n\nERROR: Trying to create a trap of unknown type, bailing out.\n";
        std::cerr << "Sourcefile: " << __FILE__ << "\tLine: " << __LINE__
        << "\n\n\n";
        exit(1);
    }

    tempTrap->setControllingSeat(nControllingSeat);

    tempTrap->setMeshName(getMeshNameFromTrapType(nType));
    tempTrap->mType = nType;

    if(nameToUse.empty())
        tempTrap->buildUniqueName();
    else
        tempTrap->setName(nameToUse);

    for (unsigned int i = 0; i < nCoveredTiles.size(); ++i)
        tempTrap->addCoveredTile(nCoveredTiles[i]);

    return tempTrap;
}

void Trap::setupTrap(GameMap* gameMap, Trap* newTrap, Player* player)
{
    gameMap->addTrap(newTrap);

    if(gameMap->isServerGameMap())
    {
        try
        {
            std::vector<Tile*> coveredTiles = newTrap->getCoveredTiles();
            int intType = static_cast<int32_t>(newTrap->getType());
            ServerNotification *serverNotification = new ServerNotification(
                ServerNotification::buildTrap, player);
            serverNotification->packet << intType << player->getSeat()->getColor() << coveredTiles.size();
            for(std::vector<Tile*>::iterator it = coveredTiles.begin(); it != coveredTiles.end(); ++it)
            {
                Tile* tile = *it;
                serverNotification->packet << tile;
            }
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
        catch (std::bad_alloc&)
        {
            Ogre::LogManager::getSingleton().logMessage("ERROR: bad alloc in RoomTreasury::destroyMeshesForTile", Ogre::LML_CRITICAL);
            exit(1);
        }
    }

    newTrap->createMesh();

    SoundEffectsHelper::getSingleton().playInterfaceSound(
            SoundEffectsHelper::BUILDTRAP, false);
}

Trap* Trap::createTrapFromStream(GameMap* gameMap, const std::string& trapMeshName, std::istream &is,
    const std::string& trapName)
{
    Trap tempTrap(gameMap);
    tempTrap.setMeshName(trapMeshName);
    is >> &tempTrap;

    Trap *returnTrap = createTrap(gameMap, tempTrap.mType, tempTrap.mCoveredTiles,
        tempTrap.getControllingSeat(), trapName);
    return returnTrap;
}

Trap* Trap::createTrapFromPacket(GameMap* gameMap, const std::string& trapMeshName, ODPacket &is,
    const std::string& trapName)
{
    Trap tempTrap(gameMap);
    tempTrap.setMeshName(trapMeshName);
    is >> &tempTrap;

    Trap *returnTrap = createTrap(gameMap, tempTrap.mType, tempTrap.mCoveredTiles,
        tempTrap.getControllingSeat(), trapName);
    return returnTrap;
}

std::string Trap::getMeshNameFromTrapType(TrapType t)
{
    switch (t)
    {
        case nullTrapType:
            return "NullTrapType";

        case cannon:
            return "Cannon";

        case boulder:
            return "Boulder";

        default:
            return "UnknownTrapType";
    }
}

Trap::TrapType Trap::getTrapTypeFromMeshName(std::string s)
{
    if (s.compare("Cannon") == 0)
        return cannon;
    else if (s.compare("Boulder") == 0)
        return boulder;
    else if (s.compare("NullTrapType") == 0)
        return nullTrapType;
    else
    {
        std::cerr
        << "\n\n\nERROR:  Trying to get trap type from unknown mesh name, bailing out.\n";
        std::cerr << "Sourcefile: " << __FILE__ << "\tLine: " << __LINE__
        << "\n\n\n";
        exit(1);
    }
}

int Trap::costPerTile(TrapType t)
{
    switch (t)
    {
        case nullTrapType:
            return 0;

        case cannon:
            return 500;

        case boulder:
            return 500;

        default:
            return 100;
    }
}

bool Trap::doUpkeep()
{
    if(mReloadTimeCounter > 0)
    {
        mReloadTimeCounter--;
        return true;
    }

    std::vector<GameEntity*> enemyAttacked = aimEnemy();

    damage(enemyAttacked);

    if(!enemyAttacked.empty())
    {
        if(mReloadTime >= 0)
        {
            // Begin the reload countdown.
            mReloadTimeCounter = mReloadTime;
        }
        else
        {
            return false;
        }
    }
    return true;
}

bool Trap::doUpkeep(Trap* t)
{
    return t->doUpkeep();
}

void Trap::damage(std::vector<GameEntity*> enemyAttacked)
{
    for(unsigned i = 0; i < enemyAttacked.size(); ++i)
    {
        enemyAttacked[i]->takeDamage(Random::Double(mMinDamage, mMaxDamage), enemyAttacked[i]->getCoveredTiles()[0]);
    }
}

std::vector<GameEntity*> Trap::aimEnemy()
{
    return std::vector<GameEntity*>();
}

void Trap::addCoveredTile(Tile* t, double nHP)
{
    mCoveredTiles.push_back(t);
    mTileHP[t] = nHP;
    t->setCoveringTrap(true);
}

void Trap::removeCoveredTile(Tile* t)
{
    for (unsigned int i = 0; i < mCoveredTiles.size(); ++i)
    {
        if (t == mCoveredTiles[i])
        {
            mCoveredTiles.erase(mCoveredTiles.begin() + i);
            t->setCoveringTrap(false);
            mTileHP.erase(t);
            break;
        }
    }
}

Tile* Trap::getCoveredTile(int index)
{
    return mCoveredTiles[index];
}

std::vector<Tile*> Trap::getCoveredTiles()
{
    return mCoveredTiles;
}

unsigned int Trap::numCoveredTiles()
{
    return mCoveredTiles.size();
}

void Trap::clearCoveredTiles()
{
    mCoveredTiles.clear();
}

double Trap::getHP(Tile *tile)
{
    //NOTE: This function is the same as Room::getHP(), consider making a base class to inherit this from.
    if (tile != NULL)
    {
        return mTileHP[tile];
    }
    else
    {
        // If the tile give was NULL, we add the total HP of all the tiles in the room and return that.
        double total = 0.0;
        std::map<Tile*, double>::iterator itr = mTileHP.begin();
        while (itr != mTileHP.end())
        {
            total += itr->second;
            ++itr;
        }

        return total;
    }
}

double Trap::getDefense() const
{
    return 0;
}

void Trap::takeDamage(double damage, Tile *tileTakingDamage)
{
    mTileHP[tileTakingDamage] -= damage;
}

std::string Trap::getFormat()
{
    return "meshName\tcolor\t\tNextLine: numTiles\t\tSubsequent Lines: tileX\ttileY";
}

void Trap::buildUniqueName()
{
    setName(getMeshName() + "_" + Ogre::StringConverter::toString(
        getGameMap()->nextUniqueNumberTrap()));
}

std::istream& operator>>(std::istream& is, Trap *t)
{
    // When we read map file, the mesh type is read before building the room. That's
    // why we assume it is already set in the room
    int tilesToLoad, tempX, tempY, tempInt;

    is >> tempInt;
    t->setControllingSeat(t->getGameMap()->getSeatByColor(tempInt));

    is >> tilesToLoad;
    for (int i = 0; i < tilesToLoad; ++i)
    {
        is >> tempX >> tempY;
        Tile *tempTile = t->getGameMap()->getTile(tempX, tempY);
        if (tempTile != NULL)
        {
            t->addCoveredTile(tempTile);
            //FIXME: This next line will not be necessary when the tile color is properly set by the tile load routine.
            tempTile->setColor(t->getControllingSeat()->getColor());
            tempTile->colorDouble = 1.0;
        }
    }

    t->mType = Trap::getTrapTypeFromMeshName(t->getMeshName());
    return is;
}

std::ostream& operator<<(std::ostream& os, Trap *t)
{
    os << t->getMeshName() << "\t" << t->getName() << "\t";
    os << t->getControllingSeat()->getColor() << "\n" << t->mCoveredTiles.size() << "\n";
    for (unsigned int i = 0; i < t->mCoveredTiles.size(); ++i)
    {
        Tile *tempTile = t->mCoveredTiles[i];
        os << tempTile->x << "\t" << tempTile->y << "\n";
    }

    return os;
}

ODPacket& operator>>(ODPacket& is, Trap *t)
{
    // When we read map file, the mesh type is read before building the room. To have the
    // same behaviour, we assume the same and that it is already set in the room
    int tilesToLoad, tempX, tempY, tempInt;

    is >> tempInt;
    t->setControllingSeat(t->getGameMap()->getSeatByColor(tempInt));

    is >> tilesToLoad;
    for (int i = 0; i < tilesToLoad; ++i)
    {
        is >> tempX >> tempY;
        Tile *tempTile = t->getGameMap()->getTile(tempX, tempY);
        if (tempTile != NULL)
        {
            t->addCoveredTile(tempTile);
            //FIXME: This next line will not be necessary when the tile color is properly set by the tile load routine.
            tempTile->setColor(t->getControllingSeat()->getColor());
            tempTile->colorDouble = 1.0;
        }
        else
        {
            LogManager::getSingleton().logMessage("ERROR : trying to add trap on unkown tile "
                + Ogre::StringConverter::toString(tempX) + "," + Ogre::StringConverter::toString(tempY));
        }
    }

    t->mType = Trap::getTrapTypeFromMeshName(t->getMeshName());
    return is;
}

ODPacket& operator<<(ODPacket& os, Trap *t)
{
    os << t->getMeshName() << t->getName() << t->getControllingSeat()->getColor();
    os << t->mCoveredTiles.size();
    for (unsigned int i = 0; i < t->mCoveredTiles.size(); ++i)
    {
        Tile *tempTile = t->mCoveredTiles[i];
        os << tempTile->x << tempTile->y;
    }

    return os;
}
