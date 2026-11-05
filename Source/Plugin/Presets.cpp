#include "Presets.h"

namespace srr
{
namespace
{
    FactoryPreset P (const char* name, const char* loc, const char* s, const char* m, juce::StringArray doors = {})
    {
        FactoryPreset p;
        p.name = juce::String::fromUTF8 (name);
        p.location = loc; p.source = s; p.mic = m; p.openDoors = doors;
        return p;
    }

    template <typename Fn>
    FactoryPreset with (FactoryPreset p, Fn&& fn) { fn (p); return p; }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        std::vector<FactoryPreset> v;
        v.push_back (P ("Stan - dijalog u dnevnoj", "stan", "s_kauc", "m_dnevna", { "d_dnevna", "d_kuhinja" }));
        v.push_back (P ("Stan - iz kuhinje u dnevnu", "stan", "s_kuhinja", "m_dnevna", { "d_dnevna", "d_kuhinja" }));
        v.push_back (P ("Stan - iza vrata spavace sobe", "stan", "s_spavaca", "m_hodnik"));
        v.push_back (P ("Stan - tus u kupatilu", "stan", "s_kupatilo", "m_kupatilo"));
        v.push_back (P ("Stan - komsija na stepenistu", "stan", "s_stepeniste", "m_predsoblje", { "d_dnevna" }));
        v.push_back (P ("Stan - odjek stepenista", "stan", "s_stepeniste_gore", "m_stepeniste"));
        v.push_back (P ("Stan - TV u dnevnoj", "stan", "s_tv", "m_dnevna2", { "d_dnevna", "d_kuhinja" }));
        v.push_back (P ("Stan - garderober (mrtvo)", "stan", "s_garderober", "m_garderober"));
        v.push_back (P ("Kuca - neko vice iz dvorista", "kuca", "s_dvoriste", "m_hodnik", { "d_dnevni" }));
        v.push_back (P ("Kuca - radionica u garazi", "kuca", "s_garaza", "m_garaza"));
        v.push_back (P ("Kuca - TV iz dnevnog u kuhinji", "kuca", "s_tv", "m_kuhinja", { "d_dnevni" }));
        v.push_back (P ("Kafana - za stolom", "kafana", "s_sto1", "m_sto1", { "d_kuhinja_sank" }));
        v.push_back (P ("Kafana - bend iz toaleta", "kafana", "s_bina", "m_wc"));
        v.push_back (P ("Kafana - ozvucenje u sali", "kafana", "s_pa", "m_sredina", { "d_kuhinja_sank" }));
        v.push_back (P ("Kafana - sa ulice kroz izlog", "kafana", "s_sredina", "m_ulica"));
        v.push_back (P ("Hotel - TV komsija kroz zid", "hotel", "s_susedi_tv", "m_krevet"));
        v.push_back (P ("Hotel - kraj hodnika", "hotel", "s_hodnik_far", "m_hodnik"));
        v.push_back (P ("Hotel - u liftu", "hotel", "s_lift", "m_lift"));
        v.push_back (P ("Hotel - TV u sobi", "hotel", "s_tv", "m_krevet"));
        v.push_back (P ("Auto - vozac i suvozac", "auto", "s_suvozac", "m_vozac"));
        v.push_back (P ("Auto - radio", "auto", "s_radio", "m_vozac"));
        v.push_back (P ("Auto - neko na trotoaru (zatvoreno)", "auto", "s_trotoar", "m_vozac"));
        v.push_back (P ("Auto - otvorena vozaceva vrata", "auto", "s_trotoar", "m_vozac", { "d_vozac" }));
        v.push_back (P ("Auto - glas iz gepeka", "auto", "s_gepek", "m_zadnje"));
        v.push_back (P ("Auto - sa ulice (radio unutra)", "auto", "s_radio", "m_ulica"));
        v.push_back (P ("Studio - vokal u kabini", "studio", "s_vokal", "m_kabina"));
        v.push_back (P ("Studio - bubnjevi, room mic", "studio", "s_bubanj", "m_room"));
        v.push_back (P ("Studio - live room kroz staklo", "studio", "s_live", "m_rezija"));
        v.push_back (P ("Kancelarija - telefon na spikeru", "kancelarija", "s_telefon", "m_sredina", { "d_ulaz" }));
        v.push_back (P ("Kancelarija - sala za sastanke", "kancelarija", "s_sala", "m_sala", { "d_ulaz" }));
        v.push_back (P ("Radionica - hala", "radionica", "s_lift", "m_hala", { "d_rolo" }));
        v.push_back (P ("Radionica - radio iz kancelarije", "radionica", "s_radio", "m_kancelarija", { "d_rolo" }));

        v.push_back (with (P ("AUX - dnevna soba (Direct Off)", "stan", "s_dnevna", "m_dnevna", { "d_dnevna", "d_kuhinja" }),
                           [] (FactoryPreset& p) { p.direct = false; p.spread = 40; p.mix = 100; }));
        v.push_back (with (P ("AUX - kafana (Direct Off)", "kafana", "s_sredina", "m_sredina", { "d_kuhinja_sank" }),
                           [] (FactoryPreset& p) { p.direct = false; p.spread = 55; p.mix = 100; }));
        v.push_back (with (P ("AUX - hala (Direct Off)", "radionica", "s_hala", "m_hala", { "d_rolo" }),
                           [] (FactoryPreset& p) { p.direct = false; p.spread = 60; p.mix = 100; p.predelay = 10; }));
        v.push_back (with (P ("AUX - stepeniste (Direct Off)", "stan", "s_stepeniste", "m_stepeniste"),
                           [] (FactoryPreset& p) { p.direct = false; p.spread = 50; p.mix = 100; }));
        return v;
    }();
    return presets;
}
} // namespace srr
