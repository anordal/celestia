// celx.cpp
// 
// Copyright (C) 2003, Chris Laurel <claurel@shatters.net>
//
// Lua script extensions for Celestia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <cassert>
#include <cstring>
#include <celengine/celestia.h>
#include <celmath/vecmath.h>

// Ugh . . . the C++ standard says that stringstream should be in
// sstream, but the GNU C++ compiler uses strstream instead.
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif // HAVE_SSTREAM

#include "celx.h"
#include "celestiacore.h"
extern "C" {
#include "lualib.h"
}

using namespace std;

static const char* ClassNames[] =
{
    "class_celestia",
    "class_observer",
    "class_object",
    "class_vec3",
    "class_matrix",
    "class_rotation",
    "class_position",
    "class_frame",
};

static const int _Celestia = 0;
static const int _Observer = 1;
static const int _Object   = 2;
static const int _Vec3     = 3;
static const int _Matrix   = 4;
static const int _Rotation = 5;
static const int _Position = 6;
static const int _Frame    = 7;

#define CLASS(i) ClassNames[(i)]


// Push a class name onto the Lua stack
static void PushClass(lua_State* l, int id)
{
    lua_pushlstring(l, ClassNames[id], strlen(ClassNames[id]));
}


// Set the class (metatable) of the object on top of the stack
static void SetClass(lua_State* l, int id)
{
    PushClass(l, id);
    lua_rawget(l, LUA_REGISTRYINDEX);
    if (lua_type(l, -1) != LUA_TTABLE)
        cout << "Metatable for " << ClassNames[id] << " not found!\n";
    if (lua_setmetatable(l, -2) == 0)
        cout << "Error setting metatable for " << ClassNames[id] << '\n';
}


// Initialize the metatable for a class; sets the appropriate registry
// entries and __index, leaving the metatable on the stack when done.
static void CreateClassMetatable(lua_State* l, int id)
{
    lua_newtable(l);
    PushClass(l, id);
    lua_pushvalue(l, -2);
    lua_rawset(l, LUA_REGISTRYINDEX); // registry.name = metatable
    lua_pushvalue(l, -1);
    PushClass(l, id);
    lua_rawset(l, LUA_REGISTRYINDEX); // registry.metatable = name

    lua_pushliteral(l, "__index");
    lua_pushvalue(l, -2);
    lua_rawset(l, -3);
}


// Register a class 'method' in the metatable (assumed to be on top of the stack)
static void RegisterMethod(lua_State* l, const char* name, lua_CFunction fn)
{
    lua_pushstring(l, name);
    lua_pushvalue(l, -2);
    lua_pushcclosure(l, fn, 1);
    lua_settable(l, -3);
}

// Verify that an object at location index on the stack is of the
// specified class
static void* CheckUserData(lua_State* l, int index, int id)
{
    // get registry[metatable]
    if (!lua_getmetatable(l, index))
        return NULL;
    lua_rawget(l, LUA_REGISTRYINDEX);

    if (lua_type(l, -1) != LUA_TSTRING)
    {
        cout << "CheckUserData failed!  Unregistered class.\n";
        lua_pop(l, 1);
        return NULL;
    }

    const char* classname = lua_tostring(l, -1);
    if (classname != NULL && strcmp(classname, ClassNames[id]) == 0)
    {
        lua_pop(l, 1);
        return lua_touserdata(l, index);
    }
    else
    {
        cout << "CheckUserData failed!  Expected " << ClassNames[id] << " but got " << classname << '\n';
        lua_pop(l, 1);
        return NULL;
    }
}

// Only check for type: similar to CheckUserData, but don't report errors
static bool istype(lua_State* l, int index, int id)
{
    // get registry[metatable]
    if (!lua_getmetatable(l, index))
        return false;
    lua_rawget(l, LUA_REGISTRYINDEX);

    if (lua_type(l, -1) != LUA_TSTRING)
    {
        cout << "istype failed!  Unregistered class.\n";
        lua_pop(l, 1);
        return false;
    }

    const char* classname = lua_tostring(l, -1);
    if (classname != NULL && strcmp(classname, ClassNames[id]) == 0)
    {
        lua_pop(l, 1);
        return true;
    }
    lua_pop(l, 1);
    return false;
}

LuaState::LuaState() :
    state(NULL),
    costate(NULL),
    alive(false),
    timer(NULL)
{
    state = lua_open();
    timer = CreateTimer();
}

LuaState::~LuaState()
{
    delete timer;
    if (state != NULL)
        lua_close(state);
#if 0
    if (costate != NULL)
        lua_close(costate);
#endif
}

lua_State* LuaState::getState() const
{
    return state;
}


double LuaState::getTime() const
{
    return timer->getTime();
}

bool LuaState::createThread()
{
    // Initialize the coroutine which wraps the script
    if (!(lua_isfunction(state, -1) && !lua_iscfunction(state, -1)))
    {
        // Should never happen; we manually set up the stack in C++
        assert(0);
        return false;
    }
    else
    {
        costate = lua_newthread(state);
        if (costate == NULL)
            return false;
        lua_pushvalue(state, -2);
        lua_xmove(state, costate, 1);  /* move function from L to NL */
        alive = true;
        return true;
    }
}


string LuaState::getErrorMessage()
{
    if (lua_gettop(state) > 0)
    {
        if (lua_isstring(state, -1))
            return lua_tostring(state, -1);
    }
    return "";
}


static int auxresume(lua_State *L, lua_State *co, int narg)
{
    int status;
#if 0
    if (!lua_checkstack(co, narg))
        luaL_error(L, "too many arguments to resume");
#endif
    lua_xmove(L, co, narg);

    status = lua_resume(co, narg);
    if (status == 0)
    {
        int nres = lua_gettop(co);
#if 0
        if (!lua_checkstack(L, narg))
              luaL_error(L, "too many results to resume");
#endif
        lua_xmove(co, L, nres);  // move yielded values
        return nres;
    }
    else
    {
        lua_xmove(co, L, 1);  // move error message
        return -1;            // error flag
    }
}


bool LuaState::isAlive() const
{
    return alive;
}


struct ReadChunkInfo
{
    char* buf;
    int bufSize;
    istream* in;
};

static const char* readStreamChunk(lua_State* state, void* udata, size_t* size)
{
    assert(udata != NULL);
    if (udata == NULL)
        return NULL;

    ReadChunkInfo* info = reinterpret_cast<ReadChunkInfo*>(udata);
    assert(info->buf != NULL);
    assert(info->in != NULL);

    if (!info->in->good())
    {
        *size = 0;
        return NULL;
    }

    info->in->read(info->buf, info->bufSize);
    streamsize nread = info->in->gcount();
    
    *size = (size_t) nread;
    if (nread == 0)
        return NULL;
    else
        return info->buf;
}


static void checkArgs(lua_State* l,
                      int minArgs, int maxArgs, const char* errorMessage)
{
    int argc = lua_gettop(l);
    if (argc < minArgs || argc > maxArgs)
    {
        lua_pushstring(l, errorMessage);
        lua_error(l);
    }
}


static int parseRenderFlag(const string& name)
{
    if (compareIgnoringCase(name, "orbits") == 0)
        return Renderer::ShowOrbits;
    else if (compareIgnoringCase(name, "cloudmaps") == 0)
        return Renderer::ShowCloudMaps;
    else if (compareIgnoringCase(name, "constellations") == 0)
        return Renderer::ShowDiagrams;
    else if (compareIgnoringCase(name, "galaxies") == 0)
        return Renderer::ShowGalaxies;
    else if (compareIgnoringCase(name, "planets") == 0)
        return Renderer::ShowPlanets;
    else if (compareIgnoringCase(name, "stars") == 0)
        return Renderer::ShowStars;
    else if (compareIgnoringCase(name, "nightmaps") == 0)
        return Renderer::ShowNightMaps;
    else if (compareIgnoringCase(name, "eclipseshadows") == 0)
        return Renderer::ShowEclipseShadows;
    else if (compareIgnoringCase(name, "ringshadows") == 0)
        return Renderer::ShowRingShadows;
    else if (compareIgnoringCase(name, "comettails") == 0)
        return Renderer::ShowCometTails;
    else if (compareIgnoringCase(name, "boundaries") == 0)
        return Renderer::ShowBoundaries;
    else if (compareIgnoringCase(name, "markers") == 0)
        return Renderer::ShowMarkers;
    else if (compareIgnoringCase(name, "automag") == 0)
        return Renderer::ShowAutoMag;
    else if (compareIgnoringCase(name, "atmospheres") == 0)
        return Renderer::ShowAtmospheres;
    else if (compareIgnoringCase(name, "grid") == 0)
        return Renderer::ShowCelestialSphere;
    else
        return 0;
}


static int parseLabelFlag(const string& name)
{
    if (compareIgnoringCase(name, "planets") == 0)
        return Renderer::PlanetLabels;
    else if (compareIgnoringCase(name, "moons") == 0)
        return Renderer::MoonLabels;
    else if (compareIgnoringCase(name, "spacecraft") == 0)
        return Renderer::SpacecraftLabels;
    else if (compareIgnoringCase(name, "asteroids") == 0)
        return Renderer::AsteroidLabels;
    else if (compareIgnoringCase(name, "comets") == 0)
        return Renderer::CometLabels;
    else if (compareIgnoringCase(name, "constellations") == 0)
        return Renderer::ConstellationLabels;
    else if (compareIgnoringCase(name, "stars") == 0)
        return Renderer::StarLabels;
    else if (compareIgnoringCase(name, "galaxies") == 0)
        return Renderer::GalaxyLabels;
    else
        return 0;
}


static astro::CoordinateSystem parseCoordSys(const string& name)
{
    if (compareIgnoringCase(name, "universal") == 0)
        return astro::Universal;
    else if (compareIgnoringCase(name, "ecliptic") == 0)
        return astro::Ecliptical;
    else if (compareIgnoringCase(name, "equatorial") == 0)
        return astro::Equatorial;
    else if (compareIgnoringCase(name, "planetographic") == 0)
        return astro::Geographic;
    else if (compareIgnoringCase(name, "observer") == 0)
        return astro::ObserverLocal;
    else if (compareIgnoringCase(name, "lock") == 0)
        return astro::PhaseLock;
    else if (compareIgnoringCase(name, "chase") == 0)
        return astro::Chase;
    else
        return astro::Universal;
}


static Marker::Symbol parseMarkerSymbol(const string& name)
{
    if (compareIgnoringCase(name, "diamond") == 0)
        return Marker::Diamond;
    else if (compareIgnoringCase(name, "triangle") == 0)
        return Marker::Triangle;
    else if (compareIgnoringCase(name, "square") == 0)
        return Marker::Square;
    else if (compareIgnoringCase(name, "plus") == 0)
        return Marker::Plus;
    else if (compareIgnoringCase(name, "x") == 0)
        return Marker::X;
    else
        return Marker::Diamond;
}


static Color to_color(lua_State* l, int index)
{
    Color c(0.0f, 0.0f, 0.0f);

    if (lua_isstring(l, index))
    {
        const char* s = lua_tostring(l, index);
        Color::parse(s, c);
    }

    return c;
}


// Return the CelestiaCore object stored in the globals table
static CelestiaCore* getAppCore(lua_State* l)
{
    lua_pushstring(l, "celestia");
    lua_gettable(l, LUA_GLOBALSINDEX);
    CelestiaCore** appCore =
        static_cast<CelestiaCore**>(CheckUserData(l, -1, _Celestia));
    lua_pop(l, 1);
    
    if (appCore == NULL)
    {
        return NULL;
    }       

    return *appCore;
}


int LuaState::loadScript(istream& in, const string& streamname)
{
    char buf[4096];
    ReadChunkInfo info;
    info.buf = buf;
    info.bufSize = sizeof(buf);
    info.in = &in;

    int status = lua_load(state, readStreamChunk, &info, streamname.c_str());
    if (status != 0)
        cout << "Error loading script: " << lua_tostring(state, -1) << '\n';
    
    return status;
}

int LuaState::loadScript(const string& s)
{
#ifdef HAVE_SSTREAM    
    istringstream in(s);
#else
    istrstream in(s.c_str());
#endif
    return loadScript(in, "string");
}

int LuaState::resume()
{
    assert(costate != NULL);
    if (costate == NULL)
        return false;

    lua_State* co = lua_tothread(state, -1);
    //assert(co == costate); // co can be NULL after error (top stack is errorstring)
    if (co != costate)
        return false;

    int nArgs = auxresume(state, co, 0);
    if (nArgs < 0)
    {
        alive = false;

        const char* errorMessage = lua_tostring(state, -1);

        // This is a nasty and hopefully temporary hack . . .  We continue
        // to resume the script until we get an error.  The
        // 'cannot resume dead coroutine' error appears when there were
        // no other errors, and execution terminates normally.  There
        // should be a better way to figure out whether the script ended
        // normally . . .
        if (strcmp(errorMessage, "cannot resume dead coroutine") != 0)
        {
            cout << "Error: " << errorMessage << '\n';
            CelestiaCore* appCore = getAppCore(co);
            if (appCore != NULL)
            {
                CelestiaCore::Alerter* alerter = appCore->getAlerter();
                alerter->fatalError(errorMessage);
            }
        }

        return 1; // just the error string
    }
    else
    {
        return nArgs; // arguments from yield
    }
}

static int vector_new(lua_State* l, const Vec3d& v)
{
    Vec3d* v3 = reinterpret_cast<Vec3d*>(lua_newuserdata(l, sizeof(Vec3d)));
    *v3 = v;
    SetClass(l, _Vec3);
    
    return 1;
}
    
static Vec3d* to_vector(lua_State* l, int index)
{
    return static_cast<Vec3d*>(CheckUserData(l, index, _Vec3));
}

static Vec3d* this_vector(lua_State* l)
{
    Vec3d* v3 = to_vector(l, 1);
    if (v3 == NULL)
    {
        lua_pushstring(l, "Bad vector object!");
        lua_error(l);
    }
    
    return v3;       
}

static int vector_sub(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for sub");
    Vec3d* op1 = to_vector(l, 1);
    Vec3d* op2 = to_vector(l, 2);
    if (op1 == NULL || op2 == NULL)
    {
        lua_pushstring(l, "Subtraction only defined for two vectors");
        lua_error(l);
    }
    else
    {
        Vec3d result = *op1 - *op2;
        vector_new(l, result);
    }
    return 1;    
}

static int vector_getx(lua_State* l)
{
    Vec3d* v3 = to_vector(l, 1);
    if (v3 == NULL)
    {
        lua_pushstring(l, "Bad vector object!");
        lua_error(l);
    }
    else 
    {
        lua_Number x;
        x = static_cast<lua_Number>(v3->x);
        lua_pushnumber(l, x);
    }

    return 1;
}

static int vector_gety(lua_State* l)
{
    Vec3d* v3 = to_vector(l, 1);
    if (v3 == NULL)
    {
        lua_pushstring(l, "Bad vector object!");
        lua_error(l);
    }
    else 
    {
        lua_Number y;
        y = static_cast<lua_Number>(v3->y);
        lua_pushnumber(l, y);
    }

    return 1;
}

static int vector_getz(lua_State* l)
{
    Vec3d* v3 = to_vector(l, 1);
    if (v3 == NULL)
    {
        lua_pushstring(l, "Bad vector object!");
        lua_error(l);
    }
    else 
    {
        lua_Number z;
        z = static_cast<lua_Number>(v3->z);
        lua_pushnumber(l, z);
    }

    return 1;
}

static int vector_normalize(lua_State* l)
{
    Vec3d* v = to_vector(l, 1);
    if (v == NULL)
    {
        lua_pushstring(l, "Bad vector object!");
        lua_error(l);
    }
    else
    {
        Vec3d vn(*v);
        vn.normalize();
        vector_new(l, vn);
    }
    return 1;
}

// need these to be declared here for vector_add -- ugly
static UniversalCoord* to_position(lua_State* l, int index);
static int position_new(lua_State* l, const UniversalCoord& uc);

static int vector_add(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for addition");
    Vec3d* v1 = NULL;    
    Vec3d* v2 = NULL;
    UniversalCoord* p = NULL;
    
    if (istype(l, 1, _Vec3) && istype(l, 2, _Vec3)) 
    {
        v1 = to_vector(l, 1);
        v2 = to_vector(l, 2);
        vector_new(l, *v1 + *v2);
    }
    else
    if (istype(l, 1, _Vec3) && istype(l, 2, _Position))        
    {
        v1 = to_vector(l, 1);
        p = to_position(l, 2);
        position_new(l, *p + *v1);
    }
    else
    {
        lua_pushstring(l, "Bad vector addition!");
        lua_error(l);
    }
    return 1;    
}

static int vector_mult(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for multiplication");
    Vec3d* v1 = NULL;
    Vec3d* v2 = NULL;
    lua_Number s = 0.0;
    if (istype(l, 1, _Vec3) && istype(l, 2, _Vec3)) 
    {
        v1 = to_vector(l, 1);
        v2 = to_vector(l, 2);
        lua_pushnumber(l, *v1 * *v2);
    }
    else
    if (istype(l, 1, _Vec3) && lua_isnumber(l, 2))        
    {
        v1 = to_vector(l, 1);
        s = lua_tonumber(l, 2);
        vector_new(l, *v1 * s);
    }
    else
    if (lua_isnumber(l, 1) && istype(l, 2, _Vec3))
    {
        s = lua_tonumber(l, 1);
        v1 = to_vector(l, 2);
        vector_new(l, *v1 * s);
    }
    else
    {
        lua_pushstring(l, "Bad vector multiplication!");
        lua_error(l);
    }
    return 1;
}

static int vector_cross(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for multiplication");
    Vec3d* v1 = NULL;
    Vec3d* v2 = NULL;
    if (istype(l, 1, _Vec3) && istype(l, 2, _Vec3)) 
    {
        v1 = to_vector(l, 1);
        v2 = to_vector(l, 2);
        vector_new(l, *v1 ^ *v2);
    }
    else
    {
        lua_pushstring(l, "Bad vector multiplication!");
        lua_error(l);
    }
    return 1;

}

static int vector_tostring(lua_State* l)
{
    lua_pushstring(l, "[Vector]");
    return 1;
}


static void CreateVectorMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Vec3);
    
    RegisterMethod(l, "__tostring", vector_tostring);
    RegisterMethod(l, "__add", vector_add);
    RegisterMethod(l, "__sub", vector_sub);
    RegisterMethod(l, "__mul", vector_mult);
    RegisterMethod(l, "__pow", vector_cross);
    RegisterMethod(l, "getx", vector_getx);
    RegisterMethod(l, "gety", vector_gety);
    RegisterMethod(l, "getz", vector_getz);
    RegisterMethod(l, "normalize", vector_normalize);

    lua_pop(l, 1); // remove metatable from stack
}

static int rotation_new(lua_State* l, const Quatd& qd)
{
    Quatd* q = reinterpret_cast<Quatd*>(lua_newuserdata(l, sizeof(Quatd)));
    *q = qd;
    SetClass(l, _Rotation);
    
    return 1;
}

static Quatd* to_rotation(lua_State* l, int index)
{
    return static_cast<Quatd*>(CheckUserData(l, index, _Rotation));
}

static Quatd* this_rotation(lua_State* l)
{
    Quatd* q = to_rotation(l, 1);
    if (q == NULL)
    {
        lua_pushstring(l, "Bad rotation object!");
        lua_error(l);
    }
    
    return q;       
}

static int rotation_add(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for add");
    Quatd* q1 = to_rotation(l, 1);
    Quatd* q2 = to_rotation(l, 2);
    if (q1 == NULL || q2 == NULL)
    {
        lua_pushstring(l, "Addition only defined for two rotations");
        lua_error(l);
    }
    else
    {
        Quatd result = *q1 + *q2;
        rotation_new(l, result);
    }
    return 1;    
}

static int rotation_mult(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for multiplication");
    Quatd* r1 = NULL;
    Quatd* r2 = NULL;
    Vec3d* v = NULL;
    lua_Number s = 0.0;
    if (istype(l, 1, _Rotation) && istype(l, 2, _Rotation)) 
    {
        r1 = to_rotation(l, 1);
        r2 = to_rotation(l, 2);
        rotation_new(l, *r1 * *r2);
    }
    else
    if (istype(l, 1, _Rotation) && lua_isnumber(l, 2))        
    {
        r1 = to_rotation(l, 1);
        s = lua_tonumber(l, 2);
        rotation_new(l, *r1 * s);
    }
    else
    if (lua_isnumber(l, 1) && istype(l, 2, _Rotation))
    {
        s = lua_tonumber(l, 1);
        r1 = to_rotation(l, 2);
        rotation_new(l, *r1 * s);
    }
    else
    if (istype(l, 1, _Rotation) && istype(l, 2, _Vec3))
    {
        v = to_vector(l, 1);
        r1 = to_rotation(l, 2);
        rotation_new(l, *v * *r1);
    }
    else
    {
        lua_pushstring(l, "Bad rotation multiplication!");
        lua_error(l);
    }
    return 1;
}       

static int rotation_tostring(lua_State* l)
{
    lua_pushstring(l, "[Rotation]");
    return 1;
}

static void CreateRotationMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Rotation);
    
    RegisterMethod(l, "__tostring", rotation_tostring);
    RegisterMethod(l, "__add", rotation_add);
    RegisterMethod(l, "__mul", rotation_mult);

    lua_pop(l, 1); // remove metatable from stack
}


// position - a 128-bit per component universal coordinate
static int position_new(lua_State* l, const UniversalCoord& uc)
{
    UniversalCoord* ud = reinterpret_cast<UniversalCoord*>(lua_newuserdata(l, sizeof(UniversalCoord)));
    *ud = uc;

    SetClass(l, _Position);

    return 1;
}

static UniversalCoord* to_position(lua_State* l, int index)
{
    return static_cast<UniversalCoord*>(CheckUserData(l, index, _Position));
}

static UniversalCoord* this_position(lua_State* l)
{
    UniversalCoord* uc = to_position(l, 1);
    if (uc == NULL)
    {
        lua_pushstring(l, "Bad position object!");
        lua_error(l);
    }

    return uc;
}

static int position_getx(lua_State* l)
{
    UniversalCoord* uc = this_position(l);
    if (uc != NULL) 
    {
        lua_Number x;
        x = uc->x;
        lua_pushnumber(l, x);
    }
    else
    {
        lua_pushstring(l, "Bad position object!");
        lua_error(l);
    }

    return 1;        
    
}

static int position_gety(lua_State* l)
{
    UniversalCoord* uc = this_position(l);
    if (uc != NULL) 
    {
        lua_Number y;
        y = uc->y;
        lua_pushnumber(l, y);
    }
    else
    {
        lua_pushstring(l, "Bad position object!");
        lua_error(l);
    }

    return 1;        
    
}

static int position_getz(lua_State* l)
{
    UniversalCoord* uc = this_position(l);
    if (uc != NULL) 
    {
        lua_Number z;
        z = uc->z;
        lua_pushnumber(l, z);
    }
    else
    {
        lua_pushstring(l, "Bad position object!");
        lua_error(l);
    }

    return 1;        
    
}

static int position_vectorto(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected to position:vectorto");
    UniversalCoord* uc = this_position(l);
    UniversalCoord* uc2 = to_position(l, 2);
    
    if (uc == NULL)
    {
        lua_pushstring(l, "Bad position object!");
        lua_error(l);
    }
    if (uc2 == NULL) 
    {
        lua_pushstring(l, "Argument to position:vectorto must be a position");
        lua_error(l);
    }
    Vec3d diff = *uc2 - *uc;
    vector_new(l, diff);    
    return 1;            
}

static int position_tostring(lua_State* l)
{
    // TODO: print out the coordinate as it would appear in a cel:// URL
    lua_pushstring(l, "[Position]");

    return 1;
}

static int position_distanceto(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected to position:distanceto()");

    UniversalCoord* uc = this_position(l);
    UniversalCoord* uc2 = to_position(l, 2);
    if (uc2 == NULL)
    {
        lua_pushstring(l, "Position expected as argument to position:distanceto");
        lua_error(l);
    }

    Vec3d v = *uc2 - *uc;
    lua_pushnumber(l, astro::microLightYearsToKilometers(v.length()));
        
    return 1;
}

static int position_add(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for addition");
    UniversalCoord* p1 = NULL;
    UniversalCoord* p2 = NULL;
    Vec3d* v2 = NULL;
    
    if (istype(l, 1, _Position) && istype(l, 2, _Position)) 
    {
        p1 = to_position(l, 1);
        p2 = to_position(l, 2);
        // this is not very intuitive, as p1-p2 is a vector
        position_new(l, *p1 + *p2);
    }
    else
    if (istype(l, 1, _Position) && istype(l, 2, _Vec3))        
    {
        p1 = to_position(l, 1);
        v2 = to_vector(l, 2);
        position_new(l, *p1 + *v2);
    }
    else
    {
        lua_pushstring(l, "Bad position addition!");
        lua_error(l);
    }
    return 1;    
}

static int position_sub(lua_State* l)
{
    checkArgs(l, 2, 2, "Need two operands for subtraction");
    UniversalCoord* p1 = NULL;
    UniversalCoord* p2 = NULL;
    Vec3d* v2 = NULL;
    
    if (istype(l, 1, _Position) && istype(l, 2, _Position)) 
    {
        p1 = to_position(l, 1);
        p2 = to_position(l, 2);
        vector_new(l, *p1 - *p2);
    }
    else
    if (istype(l, 1, _Position) && istype(l, 2, _Vec3))        
    {
        p1 = to_position(l, 1);
        v2 = to_vector(l, 2);
        position_new(l, *p1 - *v2);
    }
    else
    {
        lua_pushstring(l, "Bad position subtraction!");
        lua_error(l);
    }
    return 1;    
}

static int position_addvector(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected to position:addvector()");
    UniversalCoord* uc = this_position(l);
    Vec3d* v3d = to_vector(l, 2);
    if (v3d == NULL)
    {
        lua_pushstring(l, "Vector expected as argument to position:addvector");
        lua_error(l);
    }
    else
    if (uc != NULL && v3d != NULL)
    {
        UniversalCoord ucnew = *uc + *v3d;
        position_new(l, ucnew);        
    }
    return 1;
}
    

static void CreatePositionMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Position);
    
    RegisterMethod(l, "__tostring", position_tostring);
    RegisterMethod(l, "distanceto", position_distanceto);
    RegisterMethod(l, "vectorto", position_vectorto);
    RegisterMethod(l, "addvector", position_addvector);
    RegisterMethod(l, "__add", position_add);
    RegisterMethod(l, "getx", position_getx);
    RegisterMethod(l, "gety", position_gety);
    RegisterMethod(l, "getz", position_getz);

    lua_pop(l, 1); // remove metatable from stack
}


// frame of reference
static int frame_new(lua_State* l, const FrameOfReference& f)
{
    FrameOfReference* ud = reinterpret_cast<FrameOfReference*>(lua_newuserdata(l, sizeof(FrameOfReference)));
    *ud = f;

    SetClass(l, _Frame);

    return 1;
}

static FrameOfReference* to_frame(lua_State* l, int index)
{
    return static_cast<FrameOfReference*>(CheckUserData(l, index, _Frame));
}

// Convert from frame coordinates to universal.  The arguments are a time
// (Julian day number), position, and optionally an orientation.  If just
// a position is passed, the function returns just the converted position.
// Otherwise, the transformed position and orientation are both pushed onto
// the stack.
static int frame_from(lua_State* l)
{
    checkArgs(l, 2, 3, "Two or three arguments required for frame:from");

    FrameOfReference* frame = to_frame(l, 1);
    if (frame != NULL)
    {
        RigidTransform rt;
        UniversalCoord* uc = NULL;
        Quatd* q = NULL;
        double jd = 0.0;
        if (istype(l, 2, _Position))
        {
            uc = to_position(l, 2);
        }
        else
        if (istype(l, 2, _Rotation))
        {
            q = to_rotation(l, 2);
        }
        
        if (lua_isnumber(l, 3))
        {
             jd = lua_tonumber(l, 3);
        }
        else
        {
            CelestiaCore* appCore = getAppCore(l);
            if (appCore != NULL)
            {
                Simulation* sim = appCore->getSimulation();
                jd = sim->getTime();
            }
            else
            {
                lua_pushstring(l, "Internal error: couldn't get CelestiaCore in frame:from");
                lua_error(l);
            }
        }
        if (uc != NULL)
        {
            rt.translation = *uc;
            rt = frame->toUniversal(rt, jd);
            position_new(l, rt.translation);
        }
        else
        if (q != NULL)
        {
            rt.rotation = *q;
            rt = frame->toUniversal(rt, jd);
            rotation_new(l, rt.rotation);
        }
        else
        {
            lua_pushstring(l, "Position expected as second argument to frame:from()");
            lua_error(l);
        }

    }
    else
    {
        lua_pushstring(l, "Bad method call");
        lua_error(l);
    }

    return 1;
}


// Convert from universal to frame coordinates.  The arguments are a time
// (Julian day number), position, and optionally an orientation.  If just
// a position is passed, the function returns just the converted position.
// Otherwise, the transformed position and orientation are both pushed onto
// the stack.
static int frame_to(lua_State* l)
{
    checkArgs(l, 2, 3, "Two or three arguments required for frame:to");

    FrameOfReference* frame = to_frame(l, 1);
    if (frame != NULL)
    {
        RigidTransform rt;
        UniversalCoord* uc = NULL;
        Quatd* q = NULL;
        double jd = 0.0;
        if (istype(l, 2, _Position))
        {
            uc = to_position(l, 2);
        }
        else
        if (istype(l, 2, _Rotation))
        {
            q = to_rotation(l, 2);
        }
        
        if (lua_isnumber(l, 3))
        {
             jd = lua_tonumber(l, 3);
        }
        else
        {
            CelestiaCore* appCore = getAppCore(l);
            if (appCore != NULL)
            {
                Simulation* sim = appCore->getSimulation();
                jd = sim->getTime();
            }
            else
            {
                lua_pushstring(l, "Internal error: couldn't get CelestiaCore in frame:to");
                lua_error(l);
            }
        }
        if (uc != NULL)
        {
            rt.translation = *uc;
            rt = frame->fromUniversal(rt, jd);
            position_new(l, rt.translation);
        }
        else
        if (q != NULL)
        {
            rt.rotation = *q;
            rt = frame->fromUniversal(rt, jd);
            rotation_new(l, rt.rotation);
        }
        else
        {
            lua_pushstring(l, "Position expected as second argument to frame:to()");
            lua_error(l);
        }

    }
    else
    {
        lua_pushstring(l, "Bad method call");
        lua_error(l);
    }

    return 1;
}


static int frame_tostring(lua_State* l)
{
    // TODO: print out the actual information about the frame
    lua_pushstring(l, "[Frame]");

    return 1;
}

static void CreateFrameMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Frame);
    
    RegisterMethod(l, "__tostring", frame_tostring);
    RegisterMethod(l, "to", frame_to);
    RegisterMethod(l, "from", frame_from);

    lua_pop(l, 1); // remove metatable from stack
}


// object - star, planet, or deep-sky object
static int object_new(lua_State* l, const Selection& sel)
{
    Selection* ud = reinterpret_cast<Selection*>(lua_newuserdata(l, sizeof(Selection)));
    *ud = sel;

    SetClass(l, _Object);

    return 1;
}

static Selection* to_object(lua_State* l, int index)
{
    return static_cast<Selection*>(CheckUserData(l, index, _Object));
}

static Selection* this_object(lua_State* l)
{
    Selection* sel = to_object(l, 1);
    if (sel == NULL)
    {
        lua_pushstring(l, "Bad position object!");
        lua_error(l);
    }

    return sel;
}

static int object_tostring(lua_State* l)
{
    lua_pushstring(l, "[Object]");

    return 1;
}

static int object_radius(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No arguments expected to function object:radius");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        lua_pushnumber(l, sel->radius());
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 1;
}

static int object_type(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No arguments expected to function object:type");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        const char* tname = "unknown";
        switch (sel->getType())
        {
        case Selection::Type_Body:
            {
                int cl = sel->body()->getClassification();
                switch (cl)
                {
                case Body::Planet     : tname = "planet"; break;
                case Body::Moon       : tname = "moon"; break;
                case Body::Asteroid   : tname = "asteroid"; break;
                case Body::Comet      : tname = "comet"; break;
                case Body::Spacecraft : tname = "spacecraft"; break;
                case Body::Invisible  : tname = "invisible"; break;
                }
            }
            break;

        case Selection::Type_Star:
            tname = "star";
            break;

        case Selection::Type_DeepSky:
            // TODO: return cluster, galaxy, or nebula as appropriate
            tname = "deepsky";
            break;

        case Selection::Type_Location:
            tname = "location";
            break;

        case Selection::Type_Nil:
            tname = "null";
            break;
        }

        lua_pushstring(l, tname);
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 1;
}


static int object_name(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No arguments expected to function object:name");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        switch (sel->getType())
        {
        case Selection::Type_Body:
            lua_pushstring(l, sel->body()->getName().c_str());
            break;
        case Selection::Type_DeepSky:
            lua_pushstring(l, sel->deepsky()->getName().c_str());
            break;
        case Selection::Type_Star:
            lua_pushstring(l, getAppCore(l)->getSimulation()->getUniverse()
                           ->getStarCatalog()->getStarName(*(sel->star())).c_str());
            break;
        case Selection::Type_Location:
            lua_pushstring(l, sel->location()->getName().c_str());
            break;
        default:
            lua_pushstring(l, "?");
            break;
        }
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 1;
}


static int object_spectraltype(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No arguments expected to function object:spectraltype");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        if (sel->star() != NULL)
        {
            char buf[16];
            StellarClass sc = sel->star()->getStellarClass();
            if (sc.str(buf, sizeof(buf)))
            {
                lua_pushstring(l, buf);
            }
            else
            {
                // This should only happen if the spectral type has > 15 chars
                // (i.e. never, unless there's a bug)
                assert(0);
                lua_pushstring(l, "Bad spectral type (this is a bug!)");
                lua_error(l);
            }
        }
        else
        {
            lua_pushnil(l);
        }
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 1;
}


static int object_absmag(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No arguments expected to function object:absmag");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        if (sel->star() != NULL)
            lua_pushnumber(l, sel->star()->getAbsoluteMagnitude());
        else
            lua_pushnil(l);
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 1;
}


static int object_mark(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc < 1)
    {
        lua_pushstring(l, "Bad call to object:mark");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        Color markColor(0.0f, 1.0f, 0.0f);
        if (argc > 1)
            markColor = to_color(l, 2);
        
        Marker::Symbol markSymbol = Marker::Diamond;
        if (argc > 2 && lua_isstring(l, 3))
            markSymbol = parseMarkerSymbol(lua_tostring(l, 3));

        float markSize = 10.0f;
        if (argc > 3 && lua_isnumber(l, 4))
        {
            markSize = (float) lua_tonumber(l, 4);
            if (markSize < 1.0f)
                markSize = 1.0f;
            else if (markSize > 100.0f)
                markSize = 100.0f;
        }

        CelestiaCore* appCore = getAppCore(l);
        if (appCore != NULL)
        {
            Simulation* sim = appCore->getSimulation();
            sim->getUniverse()->markObject(*sel, markSize,
                                           markColor, markSymbol, 1);
        }
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 0;
}


static int object_unmark(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No arguments expected to function object:unmark");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        CelestiaCore* appCore = getAppCore(l);
        if (appCore != NULL)
        {
            Simulation* sim = appCore->getSimulation();
            sim->getUniverse()->unmarkObject(*sel, 1);
        }
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 0;
}


// Return the observer's current position.  A time argument is optional;
// if not provided, the current master simulation time is used.
static int object_getposition(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1 && argc != 2)
    {
        lua_pushstring(l, "Bad number of arguments to observer:getposition");
        lua_error(l);
    }

    Selection* sel = to_object(l, 1);
    if (sel != NULL)
    {
        double t = 0.0;
        if (argc == 2)
        {
            if (lua_isnumber(l, 2))
            {
                t = lua_tonumber(l, 2);
            }
            else
            {
                lua_pushstring(l, "Time expected as argument to object:getposition");
                lua_error(l);
            }
        }
        else
        {
            CelestiaCore* appCore = getAppCore(l);
            if (appCore != NULL)
                t = appCore->getSimulation()->getTime();
        }

        position_new(l, sel->getPosition(t));
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }

    return 1;
}

static int object_getchildren(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected for object:getchildren()");
    Selection* sel = this_object(l);

    CelestiaCore* appCore = getAppCore(l);
    if (appCore == NULL)
    {
        lua_pushstring(l, "Celestia instance missing!");
        lua_error(l);
    }

    Simulation* sim = appCore->getSimulation();

    lua_newtable(l);
    if (sel->star() != NULL)
    {
        SolarSystemCatalog* solarSystemCatalog = sim->getUniverse()->getSolarSystemCatalog();
        SolarSystemCatalog::iterator iter = solarSystemCatalog->find(sel->star()->getCatalogNumber());
        if (iter != solarSystemCatalog->end())
        {
            SolarSystem* solarSys = iter->second;
            for (int i = 0; i < solarSys->getPlanets()->getSystemSize(); i++)
            {
                Body* body = solarSys->getPlanets()->getBody(i);
                Selection satSel(body);
                object_new(l, satSel);
                lua_rawseti(l, -2, i + 1);
            }
        }
    }
    else if (sel->body() != NULL)
    {
        const PlanetarySystem* satellites = sel->body()->getSatellites();
        if (satellites != NULL && satellites->getSystemSize() != 0)
        {
            for (int i = 0; i < satellites->getSystemSize(); i++)
            {
                Body* body = satellites->getBody(i);
                Selection satSel(body);
                object_new(l, satSel);
                lua_rawseti(l, -2, i + 1);
            }
        }
    }

    return 1;
}


static void CreateObjectMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Object);

    RegisterMethod(l, "__tostring", object_tostring);
    RegisterMethod(l, "radius", object_radius);
    RegisterMethod(l, "type", object_type);
    RegisterMethod(l, "spectraltype", object_spectraltype);
    RegisterMethod(l, "absmag", object_absmag);
    RegisterMethod(l, "name", object_name);
    RegisterMethod(l, "mark", object_mark);
    RegisterMethod(l, "unmark", object_unmark);
    RegisterMethod(l, "getposition", object_getposition);
    RegisterMethod(l, "getchildren", object_getchildren);

    lua_pop(l, 1); // pop metatable off the stack
}


// observer object

static int observer_new(lua_State* l, Observer* o)
{
    Observer** ud = static_cast<Observer**>(lua_newuserdata(l, sizeof(Observer*)));
    *ud = o;

    SetClass(l, _Observer);

    return 1;
}


static Observer* to_observer(lua_State* l, int index)
{
    // TODO: need to verify that this is actually an object, not some
    // other userdata
    Observer** o = static_cast<Observer**>(lua_touserdata(l, index));
    if (o == NULL)
        return NULL;
    else
        return *o;
}

static Observer* this_observer(lua_State* l)
{
    Observer* obs = to_observer(l, 1);
    if (obs == NULL)
    {
        lua_pushstring(l, "Bad observer object!");
        lua_error(l);
    }

    return obs;
}


static int observer_tostring(lua_State* l)
{
    lua_pushstring(l, "[Observer]");

    return 1;
}

static int observer_setposition(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument required for setpos");
    Observer* o = to_observer(l,1);
    if (o == NULL) {
        lua_pushstring(l, "Invalid object");
        lua_error(l);
    }
    UniversalCoord* uc = to_position(l,2);
    if (uc == NULL)
    {
        lua_pushstring(l, "Argument to observer:setposition must be a position");
        lua_error(l);
    }
    o->setPosition(*uc);
    return 0;
}

static int observer_setorientation(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument required for setorientation");
    Observer* o = to_observer(l,1);
    if (o == NULL) {
        lua_pushstring(l, "Invalid object");
        lua_error(l);
    }
    Quatd* q = to_rotation(l,2);
    if (q == NULL)
    {
        lua_pushstring(l, "Argument to observer:setorientation must be a rotation");
        lua_error(l);
    }
    o->setOrientation(*q);
    return 0;
}

static int observer_rotate(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument required for rotate");
    Observer* o = to_observer(l,1);
    if (o == NULL) {
        lua_pushstring(l, "Invalid object");
        lua_error(l);
    }
    Quatd* q = to_rotation(l,2);
    if (q == NULL)
    {
        lua_pushstring(l, "Argument to observer:setpos must be a rotation");
        lua_error(l);
    }
    Quatf qf(q->w, q->x, q->y, q->z);
    o->rotate(qf);
    return 0;
}

static int observer_lookat(lua_State* l)
{
    checkArgs(l, 3, 4, "Two or three arguments required for lookat");
    int argc = lua_gettop(l);

    Observer* o = to_observer(l,1);
    if (o == NULL)
    {
        lua_pushstring(l, "Invalid object");
        lua_error(l);
    }
    UniversalCoord* from = NULL;
    UniversalCoord* to = NULL;
    Vec3d* upd = NULL;
    if (argc == 3)
    {
        to = to_position(l, 2);
        upd = to_vector(l, 3);
        if (to == NULL)
        {
            lua_pushstring(l, "Argument 1 (of 2) to observer:lookat must be of type position");
            lua_error(l);
        }
    }
    else
    if (argc == 4)
    {
        from = to_position(l, 2);
        to = to_position(l, 3);
        upd = to_vector(l, 4);

        if (to == NULL || from == NULL)
        {
            lua_pushstring(l, "Argument 1 and 2 (of 3) to observer:lookat must be of type position");
            lua_error(l);
        }
    }
    if (upd == NULL)
    {
        lua_pushstring(l, "Last argument to observer:lookat must be of type vector");
        lua_error(l);
    }
    Vec3d nd;
    if (from == NULL)
    {
        nd = (*to) - o->getPosition();
    }
    else
    {
        nd = (*to) - (*from);
    }
    // need Vec3f instead:
    Vec3f up = Vec3f(upd->x, upd->y, upd->z);
    Vec3f n = Vec3f(nd.x, nd.y, nd.z);
    
    n.normalize();
    Vec3f v = n ^ up;
    v.normalize();
    Vec3f u = v ^ n;
    Quatf qf = Quatf(Mat3f(v, u, -n));
    o->setOrientation(qf);
    return 0;
}

// First argument is the target object or position; optional second argument
// is the travel time
static int observer_goto(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc < 2)
    {
        lua_pushstring(l, "At least one argument expected to function observer:goto");
        lua_error(l);
    }

    double travelTime = 5.0;
    double startInter = 0.25;
    double endInter = 0.75;
    if (argc >= 3)
    {
        if (lua_isnumber(l, 3))
            travelTime = lua_tonumber(l, 3);
        if (lua_isnumber(l, 4))
            startInter = lua_tonumber(l, 4);
        if (lua_isnumber(l, 5))
            endInter = lua_tonumber(l, 5);
        if (startInter < 0 || startInter > 1) startInter = 0.25;
        if (endInter < 0 || endInter > 1) startInter = 0.75;
    }

    Observer* o = to_observer(l, 1);
    if (o != NULL)
    {
        // The first argument may be either an object or a position
        Selection* sel = to_object(l, 2);
        if (sel != NULL)
        {
            o->gotoSelection(*sel, travelTime, startInter, endInter, Vec3f(0, 1, 0), astro::ObserverLocal);
        }
        else
        {
            // TODO: would it be better to have a separate gotolocation
            // command?  Probably.
            UniversalCoord* uc = to_position(l, 2);
            if (uc != NULL)
            {
                RigidTransform rt = o->getSituation();
                rt.translation = *uc;
                o->gotoLocation(rt, travelTime);
            }
        }
    }

    return 0;
}


// First argument is the target object or position; optional second argument
// is the travel time
static int observer_gotolocation(lua_State* l)
{
    checkArgs(l, 2,3,"Need at least 2 or 3 arguments to gotolocation");
    int argc = lua_gettop(l);
    
    double travelTime = 5.0;
    if (argc == 3)
    {
        if (lua_isnumber(l, 3))
            travelTime = lua_tonumber(l, 3);
    }

    Observer* o = this_observer(l);
    if (o != NULL)
    {
        UniversalCoord* uc = to_position(l, 2);
        if (uc != NULL)
        {
            RigidTransform rt = o->getSituation();
            rt.translation = *uc;
            o->gotoLocation(rt, travelTime);
        }

        return 0;
    }
    else
    {
        return 1;
    }
}


static int observer_center(lua_State* l)
{
    checkArgs(l, 2, 3, "At least one argument expected to observer:center");
    int argc = lua_gettop(l);

    double travelTime = 5.0;
    if (argc >= 3)
    {
        if (lua_isnumber(l, 3))
            travelTime = lua_tonumber(l, 3);
    }

    Observer* o = this_observer(l);
    Selection* sel = to_object(l, 2);
    if (sel != NULL)
        o->centerSelection(*sel, travelTime);

    return 0;
}


static int observer_follow(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected for observer:follow");

    Observer* o = this_observer(l);
    Selection* sel = to_object(l, 2);
    if (sel != NULL)
        o->follow(*sel);

    return 0;
}


static int observer_synchronous(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected for observer:synchronous");

    Observer* o = this_observer(l);
    Selection* sel = to_object(l, 2);
    if (sel != NULL)
        o->geosynchronousFollow(*sel);

    return 0;
}


static int observer_lock(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected for observer:lock");

    Observer* o = this_observer(l);
    Selection* sel = to_object(l, 2);
    if (sel != NULL)
        o->phaseLock(*sel);

    return 0;
}


static int observer_chase(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected for observer:chase");

    Observer* o = this_observer(l);
    Selection* sel = to_object(l, 2);
    if (sel != NULL)
        o->chase(*sel);

    return 0;
}


static int observer_track(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected for observer:track");

    Observer* o = this_observer(l);

    // If the argument is nil, clear the tracked object
    if (lua_isnil(l, 2))
    {
        o->setTrackedObject(Selection());
    }
    else
    {
        // Otherwise, turn on tracking and set the tracked object
        Selection* sel = to_object(l, 2);
        if (sel != NULL)
            o->setTrackedObject(*sel);
    }

    return 0;
}


// Return true if the observer is still moving as a result of a goto, center,
// or similar command.
static int observer_travelling(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected to observer:travelling");

    Observer* o = this_observer(l);
    if (o->getMode() == Observer::Travelling)
        lua_pushboolean(l, 1);
    else
        lua_pushboolean(l, 0);

    return 1;
}


// Return the observer's current time as a Julian day number
static int observer_gettime(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected to observer:gettime");

    Observer* o = this_observer(l);
    lua_pushnumber(l, o->getTime());

    return 1;
}


// Return the observer's current position
static int observer_getposition(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected to observer:getposition");

    Observer* o = this_observer(l);
    position_new(l, o->getPosition());

    return 1;
}


static int observer_getsurface(lua_State* l)
{
    checkArgs(l, 1, 1, "One argument expected to observer:getsurface()");

    Observer* obs = this_observer(l);
    lua_pushstring(l, obs->getDisplayedSurface().c_str());

    return 1;
}


static int observer_setsurface(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected to observer:setsurface()");

    Observer* obs = this_observer(l);
    const char* s = lua_tostring(l, 2);

    if (s == NULL)
        obs->setDisplayedSurface("");
    else
        obs->setDisplayedSurface(s);

    return 0;
}

static int observer_getframe(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected for observer:getframe()");
    Observer* obs = this_observer(l);
    if (obs != NULL) 
    {
        FrameOfReference frame = obs->getFrame();
        frame_new(l, frame);
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }        
    return 1;
}

static int observer_setframe(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument required for observer:setframe()");
    Observer* obs = this_observer(l);
    if (obs != NULL) 
    {
        FrameOfReference* frame;
        frame = to_frame(l, 2);
        if (frame != NULL)
        {
            obs->setFrame(*frame);
        }
        else
        {
            lua_pushstring(l, "Argument to observer:setframe must be a frame");
            lua_error(l);
        }
    }
    else
    {
        lua_pushstring(l, "Bad object!");
        lua_error(l);
    }        
    return 0;
}            

static void CreateObserverMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Observer);
    
    RegisterMethod(l, "__tostring", observer_tostring);
    RegisterMethod(l, "goto", observer_goto);
    RegisterMethod(l, "gotolocation", observer_gotolocation);
    RegisterMethod(l, "setposition", observer_setposition);
    RegisterMethod(l, "lookat", observer_lookat);
    RegisterMethod(l, "setorientation", observer_setorientation);
    RegisterMethod(l, "rotate", observer_rotate);
    RegisterMethod(l, "center", observer_center);
    RegisterMethod(l, "follow", observer_follow);
    RegisterMethod(l, "synchronous", observer_synchronous);
    RegisterMethod(l, "chase", observer_chase);
    RegisterMethod(l, "lock", observer_lock);
    RegisterMethod(l, "track", observer_track);
    RegisterMethod(l, "travelling", observer_travelling);
    RegisterMethod(l, "getframe", observer_getframe);
    RegisterMethod(l, "setframe", observer_setframe);
    RegisterMethod(l, "gettime", observer_gettime);
    RegisterMethod(l, "getposition", observer_getposition);
    RegisterMethod(l, "getsurface", observer_getsurface);
    RegisterMethod(l, "setsurface", observer_setsurface);

    lua_pop(l, 1); // remove metatable from stack
}


// Celestia objects

static int celestia_new(lua_State* l, CelestiaCore* appCore)
{
    CelestiaCore** ud = reinterpret_cast<CelestiaCore**>(lua_newuserdata(l, sizeof(CelestiaCore*)));
    *ud = appCore;

    SetClass(l, _Celestia);

    return 1;
}


static CelestiaCore* to_celestia(lua_State* l, int index)
{
    CelestiaCore** appCore = static_cast<CelestiaCore**>(CheckUserData(l, index, _Celestia));
    if (appCore == NULL)
        return NULL;
    else
        return *appCore;
}

static CelestiaCore* this_celestia(lua_State* l)
{
    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore == NULL)
    {
        lua_pushstring(l, "Bad celestia object!");
        lua_error(l);
    }

    return appCore;
}

static int celestia_flash(lua_State* l)
{
    checkArgs(l, 2, 3, "One or two arguments expected to function celestia:flash");
    CelestiaCore* appCore = this_celestia(l);
    int argc = lua_gettop(l);
    const char* s = lua_tostring(l, 2);
    double duration = 1.5;
    if (argc > 2) 
    {
        duration = lua_tonumber(l, 3);
        if (duration < 0.0) 
        {
            duration = 1.5;
        }
    }

    if (appCore != NULL && s != NULL)
        appCore->flash(s, duration);

    return 0;
}

static int celestia_print(lua_State* l)
{
    checkArgs(l, 2, 7, "One to six arguments expected to function celestia:print");
    CelestiaCore* appCore = this_celestia(l);
    int argc = lua_gettop(l);
    const char* s = lua_tostring(l, 2);
    if (s == NULL)
    {
        lua_pushstring(l, "First argument to celestia:print must be a string");
        lua_error(l);
    }
    double duration = 1.5;    
    int horig = -1;
    int vorig = -1;
    int hoff = 0;
    int voff = 5;
    
    if (argc > 2) 
    {
        duration = lua_tonumber(l, 3);
        if (duration < 0.0) 
        {
            duration = 1.5;
        }
    }

    if (argc > 3) 
    {
        horig = static_cast<int>(lua_tonumber(l, 4));
    }

    if (argc > 4) 
    {
        vorig = static_cast<int>(lua_tonumber(l, 5));
    }

    if (argc > 5) 
    {
        hoff = static_cast<int>(lua_tonumber(l, 6));
    }

    if (argc > 6) 
    {
        voff = static_cast<int>(lua_tonumber(l, 7));
    }

    if (appCore != NULL && s != NULL)
        appCore->showText(s, horig, vorig, hoff, voff, duration);

    return 0;
}


static int celestia_show(lua_State* l)
{
    checkArgs(l, 1, 1000, "Bad method call!");
    CelestiaCore* appCore = this_celestia(l);

    int argc = lua_gettop(l);
    int flags = 0;
    for (int i = 2; i <= argc; i++)
    {
        const char* s = lua_tostring(l, i);
        if (s != NULL)
            flags |= parseRenderFlag(s);
    }

    Renderer* r = appCore->getRenderer();
    r->setRenderFlags(r->getRenderFlags() | flags);

    return 0;
}


static int celestia_hide(lua_State* l)
{
    checkArgs(l, 1, 1000, "Bad method call!");
    CelestiaCore* appCore = this_celestia(l);

    int argc = lua_gettop(l);
    int flags = 0;
    for (int i = 2; i <= argc; i++)
    {
        const char* s = lua_tostring(l, i);
        if (s != NULL)
            flags |= parseRenderFlag(s);
    }

    Renderer* r = appCore->getRenderer();
    r->setRenderFlags(r->getRenderFlags() & ~flags);

    return 0;
}


static int celestia_showlabel(lua_State* l)
{
    checkArgs(l, 1, 1000, "Bad method call!");
    CelestiaCore* appCore = this_celestia(l);

    int argc = lua_gettop(l);
    int flags = 0;
    for (int i = 2; i <= argc; i++)
    {
        const char* s = lua_tostring(l, i);
        if (s != NULL)
            flags |= parseLabelFlag(s);
    }

    Renderer* r = appCore->getRenderer();
    r->setLabelMode(r->getLabelMode() | flags);

    return 0;
}


static int celestia_hidelabel(lua_State* l)
{
    checkArgs(l, 1, 1000, "Bad method call!");
    CelestiaCore* appCore = this_celestia(l);

    int argc = lua_gettop(l);
    int flags = 0;
    for (int i = 2; i <= argc; i++)
    {
        const char* s = lua_tostring(l, i);
        if (s != NULL)
            flags |= parseLabelFlag(s);
    }

    Renderer* r = appCore->getRenderer();
    r->setLabelMode(r->getLabelMode() & ~flags);

    return 0;
}


static int celestia_getobserver(lua_State* l)
{
    checkArgs(l, 1, 2, "Wrong number of arguments to celestia:getobserver()");

    CelestiaCore* appCore = this_celestia(l);
    Observer* o = appCore->getSimulation()->getActiveObserver();
    if (o == NULL)
        lua_pushnil(l);
    else
        observer_new(l, o);

    return 1;
}


static int celestia_getselection(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected to celestia:getselection()");
    CelestiaCore* appCore = this_celestia(l);
    Selection sel = appCore->getSimulation()->getSelection();
    object_new(l, sel);

    return 1;
}


static int celestia_find(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 2 || !lua_isstring(l, 2))
    {
        lua_pushstring(l, "One string argument expected for function celestia:find()");
        lua_error(l);
    }

    CelestiaCore* appCore = this_celestia(l);
    if (appCore != NULL)
    {
        Simulation* sim = appCore->getSimulation();
        // Should use universe not simulation for finding objects
        Selection sel = sim->findObjectFromPath(lua_tostring(l, 2));
        object_new(l, sel);
    }

    return 1;
}


static int celestia_select(lua_State* l)
{
    checkArgs(l, 2, 2, "One argument expected for celestia:select()");
    CelestiaCore* appCore = this_celestia(l);

    Simulation* sim = appCore->getSimulation();
    Selection* sel = to_object(l, 2);

    // If the argument is an object, set the selection; if it's anything else
    // clear the selection.
    if (sel != NULL)
        sim->setSelection(*sel);
    else
        sim->setSelection(Selection());

    return 0;
}

static int celestia_mark(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 2)
    {
        lua_pushstring(l, "One argument expected to function celestia:mark");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        Simulation* sim = appCore->getSimulation();
        Selection* sel = to_object(l, 2);

        if (sel != NULL)
        {
            sim->getUniverse()->markObject(*sel, 10.0f,
                                           Color(0.0f, 1.0f, 0.0f), Marker::Diamond, 1);
        }
    }

    return 0;
}


static int celestia_unmark(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 2)
    {
        lua_pushstring(l, "One argument expected to function celestia:unmark");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        Simulation* sim = appCore->getSimulation();
        Selection* sel = to_object(l, 2);

        if (sel != NULL)
            sim->getUniverse()->unmarkObject(*sel, 1);
    }

    return 0;
}


static int celestia_gettime(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No argument expected to function celestia:gettime");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        Simulation* sim = appCore->getSimulation();
        lua_pushnumber(l, sim->getTime());
    }

    return 1;
}


static int celestia_gettimescale(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 1)
    {
        lua_pushstring(l, "No argument expected to function celestia:gettimescale");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
        lua_pushnumber(l, appCore->getSimulation()->getTimeScale());

    return 1;
}


static int celestia_settime(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 2)
    {
        lua_pushstring(l, "One argument expected to function celestia:settime");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        if (lua_isnumber(l, 2))
            appCore->getSimulation()->setTime(lua_tonumber(l, 2));
    }

    return 0;
}


static int celestia_settimescale(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 2)
    {
        lua_pushstring(l, "One argument expected to function celestia:settimescale");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        if (lua_isnumber(l, 2))
            appCore->getSimulation()->setTimeScale(lua_tonumber(l, 2));
    }

    return 0;
}


static int celestia_tojulianday(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc < 2 || argc > 7)
    {
        lua_pushstring(l, "Wrong number of arguments to function celestia:tojulianday");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        int year = 0;
        int month = 1;
        int day = 1;
        int hour = 0;
        int minute = 0;
        double seconds = 0.0;

        // Require that all arguments are numeric
        for (int i = 2; i < argc; i++)
        {
            if (!lua_isnumber(l, i))
            {
                lua_pushstring(l, "Arguments of celestia:tojulianday must be numberic");
                lua_error(l);
            }
        }

        year = (int) lua_tonumber(l, 2);
        if (argc > 2)
            month = (int) lua_tonumber(l, 3);
        if (argc > 3)
            day = (int) lua_tonumber(l, 4);
        if (argc > 4)
            hour = (int) lua_tonumber(l, 5);
        if (argc > 5)
            minute = (int) lua_tonumber(l, 6);
        if (argc > 6)
            seconds = lua_tonumber(l, 7);

        astro::Date date(year, month, day);
        date.hour = hour;
        date.minute = minute;
        date.seconds = seconds;

        double jd = (double) date;

        lua_pushnumber(l, jd);
    }

    return 1;
}


static int celestia_unmarkall(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected to function celestia:unmarkall");

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        Simulation* sim = appCore->getSimulation();
        sim->getUniverse()->unmarkAll();
    }

    return 0;
}


static int celestia_getstarcount(lua_State* l)
{
    checkArgs(l, 1, 1, "No arguments expected to function celestia:getstarcount");

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        Universe* u = appCore->getSimulation()->getUniverse();
        lua_pushnumber(l, u->getStarCatalog()->size());
    }
    else
    {
        lua_pushstring(l, "Bad celestia object");
        lua_error(l);
    }

    return 1;
}


static int celestia_getstar(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc != 2)
    {
        lua_pushstring(l, "One argument expected to function celestia:getstar");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        if (lua_isnumber(l, 2))
        {
            double starIndex = lua_tonumber(l, 2);
            Universe* u = appCore->getSimulation()->getUniverse();
            Star* star = u->getStarCatalog()->getStar((uint32) starIndex);
            if (star == NULL)
                lua_pushnil(l);
            else
                object_new(l, Selection(star));
        }
        else
        {
            lua_pushnil(l);
        }
    }
    else
    {
        lua_pushstring(l, "Bad celestia object");
        lua_error(l);
    }

    return 1;
}

static int celestia_newvector(lua_State* l)
{
    checkArgs(l, 4, 4, "Need 3 arguments for celestia:newvector");
    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        for (int i = 2; i < 5; i++) 
        {
            if (!lua_isnumber(l, i))
            {
                lua_pushstring(l, "newvector: argument must be a number");
                lua_error(l);
            }
        }
        double x, y, z;
        x = lua_tonumber(l, 2);
        y = lua_tonumber(l, 3);
        z = lua_tonumber(l, 4);
        vector_new(l, Vec3d(x,y,z));
    }
    else
    {
        lua_pushstring(l, "Bad celestia object");
        lua_error(l);
    }

    return 1;
}

static int celestia_newrotation(lua_State* l)
{
    checkArgs(l, 3, 3, "Need 2 arguments for celestia:newrotation");
    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        Vec3d* v = to_vector(l, 2);
        if (v == NULL)
        {
            lua_pushstring(l, "newrotation: first argument must be a vector");
            lua_error(l);
        }
        double angle = 0.0;
        if (!lua_isnumber(l, 3))
        {
            lua_pushstring(l, "newrotation: second argument must be a number");
            lua_error(l);
        }
        angle = static_cast<double>(lua_tonumber(l, 3));
        Quatd q;
        q.setAxisAngle(*v, angle); 
        rotation_new(l, q);
    }
    else
    {
        lua_pushstring(l, "Bad celestia object");
        lua_error(l);
    }

    return 1;
}

static int celestia_getscripttime(lua_State* l)
{
    lua_pushstring(l, "celestia-luastate");
    lua_gettable(l, LUA_REGISTRYINDEX);
    if (!lua_islightuserdata(l, -1))
    {
        lua_pushstring(l, "Invalid table entry in celestia:getscripttime (internal error)");
        lua_error(l);
    }
    LuaState* luastate_ptr = static_cast<LuaState*>(lua_touserdata(l, -1));
    if (luastate_ptr == NULL)
    {
        lua_pushstring(l, "Invalid value in celestia:getscripttime (internal error)");
        lua_error(l);
    }
    lua_pushnumber(l, luastate_ptr->getTime()); 
    return 1;        
}

static int celestia_newframe(lua_State* l)
{
    int argc = lua_gettop(l);
    if (argc < 2)
    {
        lua_pushstring(l, "At least argument expected to function celestia:newframe");
        lua_error(l);
    }

    CelestiaCore* appCore = to_celestia(l, 1);
    if (appCore != NULL)
    {
        if (!lua_isstring(l, 2))
        {
            lua_pushstring(l, "newframe: first argument must be a string");
            lua_error(l);
        }
        else
        {
            const char* coordsysName = lua_tostring(l, 2);
            astro::CoordinateSystem coordSys = parseCoordSys(coordsysName);
            Selection* ref = NULL;
            Selection* target = NULL;

            if (coordSys == astro::PhaseLock)
            {
                if (argc >= 4)
                {
                    ref = to_object(l, 3);
                    target = to_object(l, 4);
                }

                if (ref == NULL || target == NULL)
                {
                    lua_pushstring(l, "newframe: two objects required for lock frame");
                    lua_error(l);
                } else {
                    frame_new(l, FrameOfReference(coordSys, *ref, *target));
                }
            }
            else
            {
                if (argc >= 3)
                    ref = to_object(l, 3);
                if (ref == NULL)
                {
                    lua_pushstring(l, "newframe: one object argument required for frame");
                    lua_error(l);
                } else {
                    frame_new(l, FrameOfReference(coordSys, *ref));
                }
            }   
        }
    }
    else
    {
        lua_pushstring(l, "Bad celestia object");
        lua_error(l);
    }

    return 1;
}


static int celestia_tostring(lua_State* l)
{
    lua_pushstring(l, "[Celestia]");

    return 1;
}


static void CreateCelestiaMetaTable(lua_State* l)
{
    CreateClassMetatable(l, _Celestia);

    RegisterMethod(l, "__tostring", celestia_tostring);
    RegisterMethod(l, "flash", celestia_flash);
    RegisterMethod(l, "print", celestia_print);
    RegisterMethod(l, "show", celestia_show);
    RegisterMethod(l, "hide", celestia_hide);
    RegisterMethod(l, "showlabel", celestia_showlabel);
    RegisterMethod(l, "hidelabel", celestia_hidelabel);
    RegisterMethod(l, "getobserver", celestia_getobserver);
    RegisterMethod(l, "getselection", celestia_getselection);
    RegisterMethod(l, "find", celestia_find);
    RegisterMethod(l, "select", celestia_select);
    RegisterMethod(l, "mark", celestia_mark);
    RegisterMethod(l, "unmark", celestia_unmark);
    RegisterMethod(l, "unmarkall", celestia_unmarkall);
    RegisterMethod(l, "gettime", celestia_gettime);
    RegisterMethod(l, "settime", celestia_settime);
    RegisterMethod(l, "gettimescale", celestia_gettimescale);
    RegisterMethod(l, "settimescale", celestia_settimescale);
    RegisterMethod(l, "tojulianday", celestia_tojulianday);
    RegisterMethod(l, "getstarcount", celestia_getstarcount);
    RegisterMethod(l, "getstar", celestia_getstar);
    RegisterMethod(l, "newframe", celestia_newframe);
    RegisterMethod(l, "newvector", celestia_newvector);
    RegisterMethod(l, "newrotation", celestia_newrotation);
    RegisterMethod(l, "getscripttime", celestia_getscripttime);

    lua_pop(l, 1);
}


bool LuaState::init(CelestiaCore* appCore)
{
    // Import the base and math libraries
    lua_baselibopen(state);
    lua_mathlibopen(state);
    lua_tablibopen(state);
    lua_strlibopen(state);

    // Add an easy to use wait function, so that script writers can
    // live in ignorance of coroutines.  There will probably be a significant
    // library of useful functions that can be defined purely in Lua.
    // At that point, we'll want something a bit more robust than just
    // parsing the whole text of the library every time a script is launched
    if (loadScript("wait = function(x) coroutine.yield(x) end") != 0)
        return false;
    lua_pcall(state, 0, 0, 0); // execute it

    CreateObjectMetaTable(state);
    CreateObserverMetaTable(state);
    CreateCelestiaMetaTable(state);
    CreatePositionMetaTable(state);
    CreateVectorMetaTable(state);
    CreateRotationMetaTable(state);
    CreateFrameMetaTable(state);
    // Create the celestia object
    lua_pushstring(state, "celestia");
    celestia_new(state, appCore);
    lua_settable(state, LUA_GLOBALSINDEX);
    // add a reference to the LuaState-object in the registry    
    lua_pushstring(state, "celestia-luastate");
    lua_pushlightuserdata(state, static_cast<void*>(this));
    lua_settable(state, LUA_REGISTRYINDEX);

#if 0    
    lua_pushstring(state, "dofile");
    lua_gettable(state, LUA_GLOBALSINDEX); // function "dofile" on stack
    lua_pushstring(state, "luainit.celx"); // parameter    
    if (lua_pcall(state, 1, 0, 0) != 0) // execute it
    {
        CelestiaCore::Alerter* alerter = appCore->getAlerter();
        // copy string?!
        const char* errorMessage = lua_tostring(state, -1);
        cout << errorMessage << '\n'; cout.flush();        
        alerter->fatalError(errorMessage);
        return false;
    }
#endif
    

    return true;
}
