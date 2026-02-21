#pragma once
#include "TacoPack.h"
#include <string>

namespace TacoParser
{
void ParseXml(const std::string& xmlContent, TacoPack& out);

void ParseXmlCategories(const std::string& xmlContent, TacoPack& out);

struct TrailLoadStats
{
    int xmlTrailNodes  = 0;
    int noDataAttr     = 0;
    int fileNotFound   = 0;
    int binaryFailed   = 0;
    int noMapId        = 0;
    int noPoints       = 0;
    int loaded         = 0;
    std::string sampleMissingPath;
};

void ParseXmlPois(const std::string& xmlContent, TacoPack& out,
                  TrailLoadStats* stats = nullptr);

bool LoadTrailBinary(const std::string& absolutePath, Trail& trail);

bool LoadTrailBinaryMemory(const void* data, size_t size, Trail& trail);

std::string NormalisePath(const std::string& raw);

}
