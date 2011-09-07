#ifndef CREATUREACTION_H
#define CREATUREACTION_H

#include <string>

class Tile;
class Creature;

/*! \brief A data structure to be used in the creature AI calculations.
 *
 * 
 */
class CreatureAction
{
    public:
        enum ActionType
        {
            walkToTile, // Calculate a path to the tile and follow it each turn.
            maneuver, // Like walkToTile but used for combat situations.
            digTile, // (worker only) Dig out a tile, i.e. decrease its fullness.
            claimTile, // (worker only) "Dance" on tile to change its color.
            depositGold, // (worker only) Carry gold that has been mined to a treasury.
            attackObject, // Do damage to an attackableObject withing range, if not in range begin maneuvering.
            findHome, // Try to find a "home" tile in a quarters somewhere where the creature can sleep.
            sleep, // Try to go to its home tile to and sleep when it gets there.
            train, // Check to see if our seat controls a dojo, and if so go there to train.
            idle // Stand around doing nothing.
        };

        CreatureAction();
        CreatureAction(const ActionType nType, Tile* nTile = 0, Creature* nCreature = 0);

        inline void                 setType(const ActionType nType) { type = nType; }
        inline const ActionType     getType() const                 { return type; }

        std::string toString() const;

    private:
        ActionType  type;
        Tile*       tile;
        Creature*   creature;

};

#endif

