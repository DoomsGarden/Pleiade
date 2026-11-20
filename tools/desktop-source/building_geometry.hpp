#pragma once
#include "scene.hpp"

namespace pl {
inline constexpr float terracottaMaterial = 130.f;

// Unit hip roof. The ridge runs along Z, shortened to expose both hip slopes.
inline void hipRoofMesh(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices) {
    vertices.clear();
    indices.clear();
    const V3 fl{-.5f,-.5f,.5f}, fr{.5f,-.5f,.5f};
    const V3 bl{-.5f,-.5f,-.5f}, br{.5f,-.5f,-.5f};
    const V3 front{0,.5f,.24f}, back{0,.5f,-.24f};
    auto face=[&](V3 a,V3 b,V3 c,bool across){
        const V3 n=unit(cross(b-a,c-a));
        for(V3 p:{a,b,c}){
            indices.push_back(static_cast<uint32_t>(vertices.size()));
            vertices.push_back({p,n,(across?p.x:p.z)+.5f,p.y+.5f});
        }
    };
    face(fl,fr,front,true);
    face(br,bl,back,true);
    face(bl,fl,front,false);
    face(bl,front,back,false);
    face(fr,br,back,false);
    face(fr,back,front,false);
    const V3 eave{0,-.065f,0};
    face(fl+eave,fr+eave,fr,true);face(fl+eave,fr,fl,true);
    face(br+eave,bl+eave,bl,true);face(br+eave,bl,br,true);
    face(bl+eave,fl+eave,fl,false);face(bl+eave,fl,bl,false);
    face(fr+eave,br+eave,br,false);face(fr+eave,br,fr,false);
    face(bl+eave,br+eave,fr+eave,true);
    face(bl+eave,fr+eave,fl+eave,true);
}
}
