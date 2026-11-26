// Exercise the actual procedural material implementation without a GPU.
#define main texture_generator_entry
#include "../tools/gen_textures_4k.cpp"
#undef main
#include <cassert>

static void Check(const PbrSet& maps, int size) {
    const size_t bytes = size_t(size)*size*3;
    assert(maps.diff.size() == bytes && maps.norm.size() == bytes && maps.roughMetal.size() == bytes);
    for (size_t i=0; i<bytes; i+=3) {
        float x=maps.norm[i]/127.5f-1, y=maps.norm[i+1]/127.5f-1, z=maps.norm[i+2]/127.5f-1;
        assert(std::fabs(x*x+y*y+z*z-1) < 0.025f);
        assert(z > 0);
    }
}
static void Dielectric(const PbrSet& m) {
    for (size_t i=1; i<m.roughMetal.size(); i+=3) assert(m.roughMetal[i] == 0);
}
static void Seam(const PbrSet& m, int n) {
    // A periodic texture's last sampled row is adjacent, not identical, to its first.
    double total = 0;
    for (int i=0; i<n; ++i) for (int c=0; c<3; ++c) {
        total += std::abs(int(m.diff[(i*n)*3+c])-int(m.diff[(i*n+n-1)*3+c]));
        total += std::abs(int(m.diff[i*3+c])-int(m.diff[((n-1)*n+i)*3+c]));
    }
    assert(total/(n*6) < 12.0);
}
int main() {
    using Generator = PbrSet(*)(int);
    Generator generators[] = {genStone4K,genFloor4K,genMetalPanel4K,genBrushedMetal4K,
        genCrateWood4K,genFeather4K,genHazardMetal4K,genBanyaLog4K,genBanyaRooster4K,
        genBanyaPlank4K,genBanyaStone4K,genBanyaCopper4K,genBanyaBirch4K,
        genCasinoFelt4K,genCasinoGold4K,genCasinoMarble4K,genCasinoMahogany4K};
    for (auto gen : generators) Check(gen(128),128);
    for (int size : {256,512}) {
        auto felt=genCasinoFelt4K(size);
        Check(felt,size); Dielectric(felt); Seam(felt,size);
        for (size_t i=0; i<felt.roughMetal.size(); i+=3) assert(felt.roughMetal[i] >= 209);
        auto gold=genCasinoGold4K(size); Check(gold,size); Seam(gold,size);
        for (size_t i=0; i<gold.roughMetal.size(); i+=3) assert(gold.roughMetal[i] >= 67);
    }
    Dielectric(genCasinoMarble4K(128)); Dielectric(genCasinoMahogany4K(128));
    Dielectric(genBanyaStone4K(128)); Dielectric(genBanyaRooster4K(128));
    auto a=genCasinoFelt4K(128), b=genCasinoFelt4K(128);
    assert(a.diff==b.diff && a.norm==b.norm && a.roughMetal==b.roughMetal);
    std::puts("Material maps: sizes, normals, roughness, dielectric channels, seam continuity and determinism passed.");
}
