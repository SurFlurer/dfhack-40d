#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <vector>
#include <string>

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Maps.h"
#include "modules/Gui.h"
#include "TileTypes.h"
#include "modules/MapCache.h"
#include "df/plant.h"
#include "df/matgloss_plant.h"
#include "df/matgloss_wood.h"

using std::vector;
using std::string;
using namespace DFHack;
using df::global::world;

const uint32_t sapling_to_tree_threshold = 120 * 28 * 12 * 3; // 3 years

DFHACK_PLUGIN("plants");

enum do_what
{
    do_immolate,
    do_extirpate
};

static bool getoptions( vector <string> & parameters, bool & shrubs, bool & trees, bool & help)
{
    for(size_t i = 0;i < parameters.size();i++)
    {
        if(parameters[i] == "shrubs")
        {
            shrubs = true;
        }
        else if(parameters[i] == "trees")
        {
            trees = true;
        }
        else if(parameters[i] == "all")
        {
            trees = true;
            shrubs = true;
        }
        else if(parameters[i] == "help" || parameters[i] == "?")
        {
            help = true;
        }
        else
        {
            return false;
        }
    }
    return true;
}

/**
 * Book of Immolations, chapter 1, verse 35:
 * Armok emerged from the hellish depths and beheld the sunny realms for the first time.
 * And he cursed the plants and trees for their bloodless wood, turning them into ash and smoldering ruin.
 * Armok was pleased and great temples were built by the dwarves, for they shared his hatred for trees and plants.
 */
static command_result immolations (color_ostream &out, do_what what, bool shrubs, bool trees)
{
    CoreSuspender suspend;
    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    uint32_t x_max, y_max, z_max;
    Maps::getSize(x_max, y_max, z_max);
    MapExtras::MapCache map;
    if(shrubs || trees)
    {
        int destroyed = 0;
        for(size_t i = 0 ; i < world->plants.all.size(); i++)
        {
            df::plant *p = world->plants.all[i];
            if(shrubs && (p->type >= df::plant_type::shrub_dry) || trees && (p->type <= df::plant_type::tree_wet))
            {
                if (what == do_immolate)
                    p->flags.bits.is_burning = true;
                p->hitpoints = 0;
                destroyed ++;
            }
        }
        out.print("Praise Armok!\n");
    }
    else
    {
        int32_t x,y,z;
        if(Gui::getCursorCoords(x,y,z))
        {
            auto block = Maps::getTileBlock(x,y,z);
            stl::vector<df::plant *> *alltrees = block ? &block->plants : NULL;
            if(alltrees)
            {
                bool didit = false;
                for(size_t i = 0 ; i < alltrees->size(); i++)
                {
                    df::plant * tree = alltrees->at(i);
                    if(tree->pos.x == x && tree->pos.y == y && tree->pos.z == z)
                    {
                        if(what == do_immolate)
                            tree->flags.bits.is_burning = true;
                        tree->hitpoints = 0;
                        didit = true;
                        break;
                    }
                }
                /*
                if(!didit)
                {
                    cout << "----==== There's NOTHING there! ====----" << endl;
                }
                */
            }
        }
        else
        {
            out.printerr("No mass destruction and no cursor...\n" );
        }
    }
    return CR_OK;
}

command_result df_immolate (color_ostream &out, vector <string> & parameters, do_what what)
{
    bool shrubs = false, trees = false, help = false;
    if (getoptions(parameters, shrubs, trees, help) && !help)
    {
        return immolations(out, what, shrubs, trees);
    }

    string mode;
    if (what == do_immolate)
        mode = "Set plants on fire";
    else
        mode = "Kill plants";

    if (!help)
        out.printerr("Invalid parameter!\n");

    out << "Usage:\n" <<
        mode << " (under cursor, 'shrubs', 'trees' or 'all').\n"
        "Without any options, this command acts on the plant under the cursor.\n"
        "Options:\n"
        "shrubs   - affect all shrubs\n"
        "trees    - affect all trees\n"
        "all      - affect all plants\n";

    return CR_OK;
}

command_result df_grow (color_ostream &out, vector <string> & parameters)
{
    for(size_t i = 0; i < parameters.size();i++)
    {
        if(parameters[i] == "help" || parameters[i] == "?")
        {
            out << "Usage:\n"
                "This command turns all living saplings on the map into full-grown trees.\n"
                "With active cursor, work on the targetted one only.\n";
            return CR_OK;
        }
    }

    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    MapExtras::MapCache map;
    int32_t x,y,z;
    if(Gui::getCursorCoords(x,y,z))
    {
        auto block = Maps::getTileBlock(x,y,z);
        stl::vector<df::plant *> *alltrees = block ? &block->plants : NULL;
        if(alltrees)
        {
            for(size_t i = 0 ; i < alltrees->size(); i++)
            {
                df::plant * tree = alltrees->at(i);
                if(tree->pos.x == x && tree->pos.y == y && tree->pos.z == z)
                {
                    if(tileShape(map.tiletypeAt(DFCoord(x,y,z))) == tiletype_shape::SAPLING &&
                        tileSpecial(map.tiletypeAt(DFCoord(x,y,z))) != tiletype_special::DEAD)
                    {
                        tree->grow_counter = sapling_to_tree_threshold;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        int grown = 0;
        for(size_t i = 0 ; i < world->plants.all.size(); i++)
        {
            df::plant *p = world->plants.all[i];
            df::tiletype ttype = map.tiletypeAt(df::coord(p->pos.x,p->pos.y,p->pos.z));
            if((p->type <= df::plant_type::tree_wet) && tileShape(ttype) == tiletype_shape::SAPLING && tileSpecial(ttype) != tiletype_special::DEAD)
            {
                p->grow_counter = sapling_to_tree_threshold;
            }
        }
    }

    return CR_OK;
}

command_result df_createplant (color_ostream &out, vector <string> & parameters)
{
    if ((parameters.size() != 1) || (parameters[0] == "help" || parameters[0] == "?"))
    {
        out << "Usage:\n"
            "Create a new plant at the cursor.\n"
            "Specify the type of plant to create by its raw ID (e.g. TOWER_CAP or MUSHROOM_HELMET_PLUMP).\n"
            "Only shrubs and saplings can be placed, and they must be located on a dirt or grass floor.\n";
        return CR_OK;
    }

    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }

    int32_t x,y,z;
    if(!Gui::getCursorCoords(x,y,z))
    {
        out.printerr("No cursor detected - please place the cursor over the location in which you wish to create a new plant.\n");
        return CR_FAILURE;
    }
    df::map_block *map = Maps::getTileBlock(x, y, z);
    if (!map)
    {
        out.printerr("Invalid location selected!\n");
        return CR_FAILURE;
    }
    int tx = x & 15, ty = y & 15;
    int mat = tileMaterial(map->tiletype[tx][ty]);
    if ((tileShape(map->tiletype[tx][ty]) != tiletype_shape::FLOOR) || ((mat != tiletype_material::SOIL) && (mat != tiletype_material::GRASS_DARK) && (mat != tiletype_material::GRASS_LIGHT)))
    {
        out.printerr("Plants can only be placed on dirt or grass floors!\n");
        return CR_FAILURE;
    }

    int plant_id = -1;
    int wood_id = -1;
    df::matgloss_plant *plant_raw = NULL;
    df::matgloss_wood *wood_raw = NULL;
    for (size_t i = 0; i < world->raws.matgloss.plant.size(); i++)
    {
        plant_raw = world->raws.matgloss.plant[i];
        if (plant_raw->id == parameters[0])
        {
            plant_id = i;
            break;
        }
        plant_raw = NULL;
    }
    for (size_t i = 0; i < world->raws.matgloss.wood.size(); i++)
    {
        wood_raw = world->raws.matgloss.wood[i];
        if (wood_raw->id == parameters[0])
        {
            wood_id = i;
            break;
        }
        wood_raw = NULL;
    }
    if (plant_id != -1 && wood_id != -1)
    {
        out.printerr("Specified name matches both a tree and a shrub! Defaulting to tree...\n");
        plant_id = -1;
        plant_raw = NULL;
    }

    if (plant_id == -1 && wood_id == -1)
    {
        out.printerr("Invalid plant ID specified!\n");
        return CR_FAILURE;
    }

    df::plant *plant = df::allocate<df::plant>();
    if (wood_raw)
    {
        plant->hitpoints = 400000;
        // for now, always set "watery" for WET-permitted plants, even if they're spawned away from water
        // the proper method would be to actually look for nearby water features, but it's not clear exactly how that works
        if (wood_raw->flags.is_set(matgloss_wood_flags::WET))
            plant->type = df::plant_type::tree_wet;
        else
            plant->type = df::plant_type::tree_dry;
        plant->wood_id = wood_id;
    }
    else
    {
        plant->hitpoints = 100000;
        // see above
        if (plant_raw->flags.is_set(matgloss_plant_flags::WET))
            plant->type = df::plant_type::shrub_wet;
        else
            plant->type = df::plant_type::shrub_dry;
        plant->plant_id = plant_id;
    }
    plant->pos.x = x;
    plant->pos.y = y;
    plant->pos.z = z;
    plant->update_order = rand() % 10;
    plant->temperature_tile_tick = -1;
    plant->temperature_tile = 60001;
    plant->min_safe_temp = 9900;
    plant->max_safe_temp = 60001;

    world->plants.all.push_back(plant);
    switch (plant->type)
    {
    case df::plant_type::tree_dry: world->plants.tree_dry.push_back(plant); break;
    case df::plant_type::tree_wet: world->plants.tree_wet.push_back(plant); break;
    case df::plant_type::shrub_dry: world->plants.shrub_dry.push_back(plant); break;
    case df::plant_type::shrub_wet: world->plants.shrub_wet.push_back(plant); break;
    }
    map->plants.push_back(plant);
    if (plant->type >= df::plant_type::shrub_dry)
        map->tiletype[tx][ty] = tiletype::Shrub;
    else
        map->tiletype[tx][ty] = tiletype::Sapling;

    return CR_OK;
}

command_result df_plant (color_ostream &out, vector <string> & parameters)
{
    if (parameters.size() >= 1)
    {
        if (parameters[0] == "grow") {
            parameters.erase(parameters.begin());
            return df_grow(out, parameters);
        } else
        if (parameters[0] == "immolate") {
            parameters.erase(parameters.begin());
            return df_immolate(out, parameters, do_immolate);
        } else
        if (parameters[0] == "extirpate") {
            parameters.erase(parameters.begin());
            return df_immolate(out, parameters, do_extirpate);
        } else
        if (parameters[0] == "create") {
            parameters.erase(parameters.begin());
            return df_createplant(out, parameters);
        }
    }
    return CR_WRONG_USAGE;
}

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand("plant", "Plant creation and removal.", df_plant, false,
        "Command to create, grow or remove plants on the map. For more details, check the subcommand help :\n"
        "plant grow help      - Grows saplings into trees.\n"
        "plant immolate help  - Set plants on fire.\n"
        "plant extirpate help - Kill plants.\n"
        "plant create help    - Create a new plant.\n"));

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
