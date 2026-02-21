#include "TacoParser.h"
#include "TacoPack.h"

#include <pugixml.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <cstring>
#include <charconv>

// ─────────────────────────────────────────────────────────────────────────────
// TacoPack member implementations
// ─────────────────────────────────────────────────────────────────────────────

void MarkerAttribs::InheritFrom(const MarkerAttribs& p)
{
    if (iconFile.empty()         && !p.iconFile.empty())     iconFile      = p.iconFile;
    if (iconSize    == 1.0f      && p.iconSize    != 1.0f)   iconSize      = p.iconSize;
    if (alpha       == 1.0f      && p.alpha       != 1.0f)   alpha         = p.alpha;
    if (color       == 0xFFFFFFFF && p.color != 0xFFFFFFFF)  color         = p.color;
    if (heightOffset == 1.5f     && p.heightOffset != 1.5f)  heightOffset  = p.heightOffset;
    if (fadeNear    < 0.f        && p.fadeNear    >= 0.f)    fadeNear      = p.fadeNear;
    if (fadeFar     < 0.f        && p.fadeFar     >= 0.f)    fadeFar       = p.fadeFar;
    if (minSize     < 0.f        && p.minSize     >= 0.f)    minSize       = p.minSize;
    if (maxSize     < 0.f        && p.maxSize     >= 0.f)    maxSize       = p.maxSize;
    if (behavior    == 0         && p.behavior    != 0)      behavior      = p.behavior;
    if (trailColor  == 0xFFFFFFFF && p.trailColor != 0xFFFFFFFF) trailColor = p.trailColor;
    if (trailScale  == 1.0f      && p.trailScale  != 1.0f)  trailScale    = p.trailScale;
    if (animSpeedMult == 1.0f    && p.animSpeedMult != 1.0f) animSpeedMult = p.animSpeedMult;
    if (texture.empty()          && !p.texture.empty())       texture       = p.texture;
}

static MarkerCategory* FindImpl(std::vector<MarkerCategory>& cats,
                                const std::string& head, const std::string& tail)
{
    for (auto& c : cats)
    {
        if (_stricmp(c.name.c_str(), head.c_str()) == 0)
        {
            if (tail.empty()) return &c;
            auto dot = tail.find('.');
            std::string h = dot == std::string::npos ? tail : tail.substr(0, dot);
            std::string t = dot == std::string::npos ? ""   : tail.substr(dot + 1);
            return FindImpl(c.children, h, t);
        }
    }
    return nullptr;
}

MarkerCategory* MarkerCategory::Find(const std::string& path)
{
    auto dot = path.find('.');
    std::string h = dot == std::string::npos ? path : path.substr(0, dot);
    std::string t = dot == std::string::npos ? ""   : path.substr(dot + 1);
    return FindImpl(children, h, t);
}
const MarkerCategory* MarkerCategory::Find(const std::string& path) const
{
    return const_cast<MarkerCategory*>(this)->Find(path);
}

static MarkerCategory* FindOrCreateImpl(std::vector<MarkerCategory>& cats,
                                        const std::string& head, const std::string& tail)
{
    for (auto& c : cats)
    {
        if (_stricmp(c.name.c_str(), head.c_str()) == 0)
        {
            if (tail.empty()) return &c;
            auto dot = tail.find('.');
            std::string h = dot == std::string::npos ? tail : tail.substr(0, dot);
            std::string t = dot == std::string::npos ? ""   : tail.substr(dot + 1);
            return FindOrCreateImpl(c.children, h, t);
        }
    }
    MarkerCategory nc;
    nc.name = head;
    nc.displayName = head;
    cats.push_back(std::move(nc));
    MarkerCategory& inserted = cats.back();
    if (tail.empty()) return &inserted;
    auto dot = tail.find('.');
    std::string h = dot == std::string::npos ? tail : tail.substr(0, dot);
    std::string t = dot == std::string::npos ? ""   : tail.substr(dot + 1);
    return FindOrCreateImpl(inserted.children, h, t);
}

MarkerCategory* MarkerCategory::FindOrCreate(const std::string& path)
{
    auto dot = path.find('.');
    std::string h = dot == std::string::npos ? path : path.substr(0, dot);
    std::string t = dot == std::string::npos ? ""   : path.substr(dot + 1);
    return FindOrCreateImpl(children, h, t);
}

std::string TacoPack::ResolveFile(const std::string& packRelPath) const
{
    std::string key = TacoParser::NormalisePath(packRelPath);
    auto it = extractedFiles.find(key);
    return it != extractedFiles.end() ? it->second : std::string{};
}

static bool IsCategoryEnabledImpl(const std::vector<MarkerCategory>& cats,
                                  const std::string& head, const std::string& tail)
{
    for (const auto& c : cats)
    {
        if (_stricmp(c.name.c_str(), head.c_str()) == 0)
        {
            if (!c.enabled) return false;
            if (tail.empty()) return true;
            auto dot = tail.find('.');
            std::string h = dot == std::string::npos ? tail : tail.substr(0, dot);
            std::string t = dot == std::string::npos ? ""   : tail.substr(dot + 1);
            return IsCategoryEnabledImpl(c.children, h, t);
        }
    }
    return true; // unknown category — allow
}

bool TacoPack::IsCategoryEnabled(const std::string& typePath) const
{
    if (!enabled) return false;
    auto dot = typePath.find('.');
    std::string h = dot == std::string::npos ? typePath : typePath.substr(0, dot);
    std::string t = dot == std::string::npos ? ""       : typePath.substr(dot + 1);
    return IsCategoryEnabledImpl(categories, h, t);
}

// ─────────────────────────────────────────────────────────────────────────────
// TacoParser implementation
// ─────────────────────────────────────────────────────────────────────────────

namespace TacoParser
{

std::string NormalisePath(const std::string& raw)
{
    std::string out = raw;
    // Replace backslashes with forward slashes
    std::replace(out.begin(), out.end(), '\\', '/');
    // Lowercase
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    // Strip leading slash
    if (!out.empty() && out[0] == '/') out.erase(0, 1);
    return out;
}

// ── Safe attribute helpers ────────────────────────────────────────────────────

static float AttrFloat(const pugi::xml_node& n, const char* attr, float def)
{
    pugi::xml_attribute a = n.attribute(attr);
    if (!a) return def;
    return a.as_float(def);
}

static uint32_t AttrUInt(const pugi::xml_node& n, const char* attr, uint32_t def)
{
    pugi::xml_attribute a = n.attribute(attr);
    if (!a) return def;
    return (uint32_t)a.as_uint(def);
}

static int AttrInt(const pugi::xml_node& n, const char* attr, int def)
{
    pugi::xml_attribute a = n.attribute(attr);
    if (!a) return def;
    return a.as_int(def);
}

static std::string AttrStr(const pugi::xml_node& n, const char* attr)
{
    pugi::xml_attribute a = n.attribute(attr);
    return a ? std::string(a.as_string()) : std::string{};
}

// Parse a hex colour string (e.g. "ffffffff" or "#ffffffff") to ARGB uint32.
static uint32_t ParseColor(const pugi::xml_node& n, const char* attr, uint32_t def)
{
    std::string s = AttrStr(n, attr);
    if (s.empty()) return def;
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    if (s.size() != 6 && s.size() != 8) return def;
    uint32_t v = 0;
    for (char c : s)
    {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (c - '0');
        else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
    }
    if (s.size() == 6) v |= 0xFF000000u; // add full alpha if missing
    return v;
}

// ── Read display attribs from any XML node ────────────────────────────────────
static void ReadAttribs(const pugi::xml_node& node, MarkerAttribs& a)
{
    std::string iconFile = AttrStr(node, "iconFile");
    if (iconFile.empty()) iconFile = AttrStr(node, "icon-file");
    if (!iconFile.empty()) a.iconFile = iconFile;

    if (node.attribute("iconSize"))   a.iconSize     = AttrFloat(node, "iconSize",    1.0f);
    if (node.attribute("alpha"))      a.alpha        = AttrFloat(node, "alpha",        1.0f);
    if (node.attribute("color"))      a.color        = ParseColor(node, "color",       0xFFFFFFFF);
    if (node.attribute("heightOffset")) a.heightOffset = AttrFloat(node, "heightOffset", 1.5f);
    if (node.attribute("fadeNear"))   a.fadeNear     = AttrFloat(node, "fadeNear",    -1.0f);
    if (node.attribute("fadeFar"))    a.fadeFar      = AttrFloat(node, "fadeFar",     -1.0f);
    if (node.attribute("minSize"))    a.minSize      = AttrFloat(node, "minSize",     -1.0f);
    if (node.attribute("maxSize"))    a.maxSize      = AttrFloat(node, "maxSize",     -1.0f);
    if (node.attribute("behavior"))   a.behavior     = AttrInt(node,   "behavior",     0);
    if (node.attribute("trailColor")) a.trailColor   = ParseColor(node, "trailColor", 0xFFFFFFFF);
    if (node.attribute("trailScale")) a.trailScale   = AttrFloat(node, "trailScale",  1.0f);
    if (node.attribute("animSpeedMult")) a.animSpeedMult = AttrFloat(node, "animSpeedMult", 1.0f);
    std::string tex = AttrStr(node, "texture");
    if (!tex.empty()) a.texture = tex;
    if (node.attribute("triggerRange")) a.triggerRange = AttrFloat(node, "triggerRange", 2.0f);
    if (node.attribute("resetLength"))  a.resetLength  = AttrInt(node,  "resetLength",   0);
}

// ── Recursive MarkerCategory tree builder ────────────────────────────────────
static void BuildCategoryTree(const pugi::xml_node& xmlNode,
                              std::vector<MarkerCategory>& siblings,
                              const MarkerAttribs& parentAttribs)
{
    for (const pugi::xml_node& child : xmlNode.children("MarkerCategory"))
    {
        std::string name = AttrStr(child, "name");
        if (name.empty()) continue;

        MarkerCategory cat;
        cat.name        = name;
        cat.displayName = AttrStr(child, "displayName");
        if (cat.displayName.empty()) cat.displayName = name;
        cat.attribs     = parentAttribs;   // start with parent values
        ReadAttribs(child, cat.attribs);   // override with this node's values
        cat.enabled     = true;

        BuildCategoryTree(child, cat.children, cat.attribs);
        siblings.push_back(std::move(cat));
    }
}

// ── Collect effective attribs for a dot-separated type path ──────────────────
static MarkerAttribs ResolveTypeAttribs(const std::vector<MarkerCategory>& cats,
                                        const std::string& typePath)
{
    MarkerAttribs result;

    // Walk each segment of the type path building up accumulated attribs
    const std::vector<MarkerCategory>* level = &cats;
    std::string remaining = typePath;

    while (!remaining.empty() && level)
    {
        auto dot = remaining.find('.');
        std::string seg = dot == std::string::npos ? remaining : remaining.substr(0, dot);
        remaining       = dot == std::string::npos ? ""        : remaining.substr(dot + 1);

        const MarkerCategory* found = nullptr;
        for (const auto& c : *level)
        {
            if (_stricmp(c.name.c_str(), seg.c_str()) == 0) { found = &c; break; }
        }
        if (!found) break;
        result.InheritFrom(found->attribs);
        level = &found->children;
    }
    return result;
}

// ── Parse a <POIs> block ──────────────────────────────────────────────────────
static void ParsePois(const pugi::xml_node& poisNode, TacoPack& out)
{
    // POIs
    for (const pugi::xml_node& n : poisNode.children("POI"))
    {
        uint32_t mapId = AttrUInt(n, "MapID", 0);
        if (mapId == 0) continue;

        Poi poi;
        poi.mapId = mapId;
        poi.x     = AttrFloat(n, "xpos", 0.f);
        poi.y     = AttrFloat(n, "ypos", 0.f);
        poi.z     = AttrFloat(n, "zpos", 0.f);
        poi.type  = AttrStr(n, "type");
        poi.guid  = AttrStr(n, "GUID");

        // Resolve attribs from category tree then override with inline attribs
        poi.attribs = poi.type.empty() ? MarkerAttribs{}
                                       : ResolveTypeAttribs(out.categories, poi.type);
        ReadAttribs(n, poi.attribs);

        out.pois.push_back(std::move(poi));
    }

    // Trails
    for (const pugi::xml_node& n : poisNode.children("Trail"))
    {
        uint32_t mapId = AttrUInt(n, "MapID", 0);
        if (mapId == 0) continue;

        Trail trail;
        trail.mapId         = mapId;
        trail.type          = AttrStr(n, "type");
        trail.trailDataFile = AttrStr(n, "trailData");
        if (trail.trailDataFile.empty()) trail.trailDataFile = AttrStr(n, "TrailData");

        trail.attribs = trail.type.empty() ? MarkerAttribs{}
                                            : ResolveTypeAttribs(out.categories, trail.type);
        ReadAttribs(n, trail.attribs);

        // Load the binary trail data if the extracted file exists
        if (!trail.trailDataFile.empty())
        {
            std::string absPath = out.ResolveFile(trail.trailDataFile);
            if (!absPath.empty())
                LoadTrailBinary(absPath, trail);
        }

        out.trails.push_back(std::move(trail));
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void ParseXml(const std::string& xmlContent, TacoPack& out)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xmlContent.c_str());
    if (!result) return; // malformed XML

    // The root may be <OverlayData> directly or there may be an enclosing wrapper.
    pugi::xml_node root = doc.child("OverlayData");
    if (!root) root = doc.first_child(); // some packs omit the standard root

    // ── Build category tree from all <MarkerCategory> children ───────────────
    MarkerAttribs defaultAttribs;
    BuildCategoryTree(root, out.categories, defaultAttribs);

    // ── Also look for <MarkerCategory> children on <OverlayData> sub-nodes ───
    // Some packs nest everything under a second level root node.
    for (const pugi::xml_node& child : root.children())
    {
        std::string childName = child.name();
        if (childName == "POIs")
        {
            ParsePois(child, out);
        }
    }

    // Flat <POI>/<Trail> directly under root (non-standard but seen in the wild)
    ParsePois(root, out);
}

bool LoadTrailBinary(const std::string& absolutePath, Trail& trail)
{
    std::ifstream f(absolutePath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    auto sz = f.tellg();
    if (sz < 4) return false;

    std::vector<uint8_t> buf((size_t)sz);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    f.close();

    return LoadTrailBinaryMemory(buf.data(), buf.size(), trail);
}

bool LoadTrailBinaryMemory(const void* data, size_t size, Trail& trail)
{
    if (size < 4) return false;

    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    // First 4 bytes: map ID (can differ from the XML MapID, use XML value)
    // uint32_t fileMapId;
    // memcpy(&fileMapId, ptr, 4);
    ptr  += 4;
    size -= 4;

    if (size % 12 != 0) return false; // must be a multiple of 3 floats

    size_t count = size / 12;
    trail.points.reserve(trail.points.size() + count);

    for (size_t i = 0; i < count; ++i)
    {
        TrailPoint tp;
        memcpy(&tp.x, ptr,     4);
        memcpy(&tp.y, ptr + 4, 4);
        memcpy(&tp.z, ptr + 8, 4);
        ptr += 12;
        trail.points.push_back(tp);
    }
    return true;
}

} // namespace TacoParser
