export const biomes = {
    forest: {title: 'Bosque templado', description: 'Pinos, píceas y abedules entre claros y helechos.',
             layer: 12, tint: [.78, .95, .71], view: [2, 0, -5, .57, .27, 23]},
    desert: {title: 'Desierto', description: 'Dunas, ondulaciones de arena y afloramientos dispersos.',
             layer: 9, tint: [1.04, 1.01, .96], view: [0, 0, -12, .43, .32, 30]},
    snow: {title: 'Montañas nevadas', description: 'Crestas de roca, nieve en las laderas y una cabaña en el valle.',
           layer: 10, tint: [1, .99, 1.02], view: [3, 2.1, -19, .24, .19, 33]},
    tropical: {title: 'Trópico', description: 'Palmeras, plantas de hoja ancha y helechos en suelo húmedo.',
               layer: 12, tint: [.72, .91, .60], view: [0, 1.7, -3, .43, .07, 18]},
    bamboo: {title: 'Bosque de bambú', description: 'Cañas con nudos, copas cruzadas y sotobosque.',
             layer: 12, tint: [.76, .94, .69], view: [0, 1, -4, .62, .28, 18]}
};
const clamp = x => Math.min(1, Math.max(0, x));
const mix = (a, b, t) => a + (b - a) * t;
const smooth = t => clamp(t) ** 2 * (3 - 2 * clamp(t));
const extent = 78;
const snowCabin = [6, -13];

function hash(x) {
    x = Math.imul(x ^ x >>> 16, 0x7feb352d);
    x = Math.imul(x ^ x >>> 15, 0x846ca68b);
    return ((x ^ x >>> 16) >>> 0) / 4294967296;
}

function noise(x, z, seed) {
    const ix = Math.floor(x), iz = Math.floor(z);
    const u = smooth(x - ix), v = smooth(z - iz);
    const at = (a, b) => hash(seed ^ Math.imul(a, 0x68bc21eb) ^ Math.imul(b, 0x02e5be93));
    return mix(mix(at(ix, iz), at(ix + 1, iz), u), mix(at(ix, iz + 1), at(ix + 1, iz + 1), u), v);
}

function terrainProfile(name, seed) {
    const ground = (x, z) => (noise(x * .069, z * .069, seed) - .5) * 2.7
        + (noise(x * .24, z * .24, seed ^ 591) - .5) * .72;
    if (name === 'desert') {
        const direction = hash(seed ^ 851) * .38 + .23;
        return (x, z) => {
            const crosswind = x * Math.cos(direction) + z * Math.sin(direction);
            const parallel = -x * Math.sin(direction) + z * Math.cos(direction);
            const drift = noise(parallel * .027, crosswind * .023, seed) * 5.5 + Math.sin(parallel * .089) * .8;
            const phase = crosswind * .16 + drift;
            const dune = .5 + .44 * Math.sin(phase) + .064 * Math.sin(2 * phase + .87);
            const sand = noise(x * .026, z * .026, seed ^ 891);
            return ground(x, z) * .51 + Math.max(0, dune) ** 1.75 * (2.6 + sand * 5.8);
        };
    }
    if (name === 'snow') {
        const raw = (x, z) => {
            const range = smooth((-z - 25 + 5 * Math.sin(x * .056)) / 37);
            const warp = (noise(x * .027, z * .031, seed ^ 931) - .5) * 15;
            const crest = 1 - Math.hypot(noise((x + warp) * .036, z * .039, seed ^ 201) * 2 - 1, .067);
            const gullies = noise(x * .112 + z * .052, z * .066, seed ^ 133);
            const scree = noise(x * .23, z * .22, seed ^ 411);
            return .72 + ground(x, z) * .77 + range * (6 + crest ** 2.65 * 33 + (gullies - .47) * 5.4 + scree * 1.4);
        };
        const padHeight = raw(...snowCabin);
        return (x, z) => {
            const d = Math.hypot(x - snowCabin[0], z - snowCabin[1]);
            return mix(padHeight + .045 * (x - snowCabin[0]), raw(x, z), smooth((d - 4.4) / 4.8));
        };
    }
    if (name === 'tropical')
        return (x, z) => ground(x, z) + 4.8 * noise(x * .028, z * .029, seed ^ 107) ** 2;
    return ground;
}

function groundMesh(height, cells = 84) {
    const side = cells + 1;
    const vertices = new Float32Array(side * side * 8);
    const indices = new Uint32Array(cells * cells * 6);
    const tile = extent * 2 / cells, offset = tile * .45;
    let position = 0;
    for (let iz = 0; iz <= cells; iz++) {
        const z = -extent + iz * tile;
        for (let ix = 0; ix <= cells; ix++) {
            const x = -extent + ix * tile;
            const dx = (height(x + offset, z) - height(x - offset, z)) / (2 * offset);
            const dz = (height(x, z + offset) - height(x, z - offset)) / (2 * offset);
            const unit = 1 / Math.hypot(dx, 1, dz);
            vertices.set([x, height(x, z), z, -dx * unit, unit, -dz * unit, ix / cells, iz / cells], position);
            position += 8;
        }
    }
    let index = 0;
    for (let iz = 0; iz < cells; iz++)
        for (let ix = 0; ix < cells; ix++) {
            const a = iz * side + ix, b = a + side;
            indices.set([a, b, a + 1, a + 1, b, b + 1], index);
            index += 6;
        }
    function surfaceHeight(x, z) {
        const u = (x + extent) / tile, v = (z + extent) / tile;
        if (u < 0 || u > cells || v < 0 || v > cells) return height(x, z);
        const ix = Math.min(cells - 1, Math.floor(u)), iz = Math.min(cells - 1, Math.floor(v));
        const dx = u - ix, dz = v - iz, a = (iz * side + ix) * 8 + 1;
        const b = a + side * 8;
        return dx + dz <= 1 ? vertices[a] * (1 - dx - dz) + vertices[a + 8] * dx + vertices[b] * dz
                            : vertices[b + 8] * (dx + dz - 1) + vertices[a + 8] * (1 - dz) + vertices[b] * (1 - dx);
    }
    return {vertices, indices, surfaceHeight};
}

function matrix(x, y, z, sx, sy, sz, angle = 0) {
    const c = Math.cos(angle), s = Math.sin(angle);
    return [sx*c,0,-sx*s,0, 0,sy,0,0, sz*s,0,sz*c,0, x,y,z,1];
}

function copyPrototype(objects, prototype, x, y, z, size, angle, tint, distant) {
    if (!prototype) return;
    const c = Math.cos(angle), s = Math.sin(angle);
    for (const part of prototype) {
        if (part.visible === false || (distant && part.detail && part.section !== 7)) continue;
        const pose = part.instance.slice();
        for (let column = 0; column < 16; column += 4) {
            const px = pose[column], pz = pose[column + 2];
            pose[column] = size * (c * px + s * pz);
            pose[column + 1] *= size;
            pose[column + 2] = size * (-s * px + c * pz);
        }
        pose[12] += x;
        pose[13] += y;
        pose[14] += z;
        for (let i = 16; i < 19; i++) pose[i] *= tint;
        objects.push({mesh: part.mesh, detail: part.detail, section: part.section, instance: pose});
    }
}

function secondaryVegetation(type, seed, x, z) {
    if (type === 'tropical' || type === 'bamboo') return 'fern';
    return noise(x * .11, z * .11, seed ^ 467) > .56 ? 'fern' : 'juniper';
}

function selectCanopy(type, habitat, choice) {
    if (type === 'bamboo') return 'bamboo';
    if (type === 'tropical') return 'palm';
    if (type === 'snow') return 'spruce';
    return habitat > .57 && choice > .33 ? 'birch' : choice > .57 ? 'pine' : 'spruce';
}

function addVine(objects, mesh, from, to, diameter) {
    const axis = to.map((v, i) => v - from[i]);
    const span = Math.hypot(...axis);
    const [x, y, z] = axis.map(v => v / span);
    const lateral = Math.hypot(x, z);
    const [rx, rz] = lateral > .001 ? [-z / lateral, x / lateral] : [1, 0];
    const stride = span + diameter * .5;
    objects.push({mesh, instance: [
        rx * diameter, 0, rz * diameter, 0, x * stride, y * stride, z * stride, 0,
        -rz * y * diameter, (rz * x - rx * z) * diameter, rx * y * diameter, 0,
        ...from.map((v, i) => (v + to[i]) * .5), 1, .67, .82, .58, 1, .86, 0, 25, 0
    ]});
}

function connectVines(objects, mesh, trees, seed) {
    const degrees = trees.map(() => 0);
    const neighbors = [];
    for (let i = 0; i < trees.length; i++)
        for (let j = i + 1; j < trees.length; j++) {
            const [a, b] = [trees[i], trees[j]];
            const distance = Math.hypot(b[0] - a[0], b[2] - a[2]);
            if (distance >= 2.4 && distance <= 9.8 && Math.abs(b[1] - a[1]) < 4)
                neighbors.push({i, j, distance});
        }
    neighbors.sort((a, b) => a.distance - b.distance);

    function curve(a, b, sag, twines, phase) {
        const sideways = [a[2] - b[2], b[0] - a[0]];
        const length = Math.hypot(...sideways);
        const [nx, nz] = length < .01 ? [1, 0] : sideways.map(v => v / length);
        const points = 9 + twines * 3;
        let previous = a;
        for (let step = 1; step <= points; step++) {
            const t = step / points, middle = Math.sin(Math.PI * t);
            const turn = phase + t * (twines ? 13 : 5);
            const offset = Math.sin(turn) * middle * (twines ? .116 : .135);
            const point = [mix(a[0], b[0], t) + nx * offset,
                           mix(a[1], b[1], t) - sag * middle + .052 * middle * Math.cos(turn),
                           mix(a[2], b[2], t) + nz * offset];
            addVine(objects, mesh, previous, point, (twines ? .042 : .055) * (1 - .29 * t));
            previous = point;
        }
    }

    let links = 0, hanging = 0;
    for (const {i, j, distance} of neighbors) {
        if (degrees[i] >= 2 || degrees[j] >= 2) continue;
        const [a, b] = [trees[i], trees[j]];
        const phase = hash(seed ^ Math.imul(i + 1, j + 433)) * Math.PI * 2;
        const entwined = distance < 7.3 && (degrees[i] || degrees[j] || phase < 2.3);
        curve(a, b, .19 * distance, entwined, phase);
        if (entwined) curve(a, b, .19 * distance + .014, entwined, phase + Math.PI);
        degrees[i]++; degrees[j]++; links++;
    }
    for (const [i, a] of trees.entries()) {
        if (degrees[i] > 0) continue;
        const side = hash(seed ^ Math.imul(i + 1, 851));
        curve(a, [a[0] + side * .62 + .33, a[1] - 1.1 - side * 2.1, a[2] - side * .42], .08, false, side * 7);
        hanging++;
    }
    return {links, hanging};
}

export function createBiome(name, seed, data) {
    const biome = biomes[name];
    if (!biome) throw new RangeError('Bioma desconocido');
    const terrain = groundMesh(terrainProfile(name, seed), name === 'snow' ? 96 : 84);
    const height = terrain.surfaceHeight;
    const objects = [{mesh: 'biome-ground', instance: [
        ...matrix(0, 0, 0, 1, 1, 1), ...biome.tint, 1, .93, 0, 40 + biome.layer, 0
    ]}];
    const spacing = {bamboo: 8, desert: 13, tropical: 7.6}[name] ?? 10;
    const crowns = [];
    let plants = 0, rocks = 0;
    for (let row = -5; row <= 3; row++)
        for (let col = -4; col <= 4; col++) {
            const cell = (151 + row * 13 + col) ^ seed;
            const offsetX = (hash(cell ^ 191) - .5) * spacing * .75;
            const offsetZ = (hash(cell ^ 4245) - .5) * spacing * .75;
            const x = col * spacing + offsetX, z = row * spacing + offsetZ;
            const habitat = noise(x * .054, z * .054, seed ^ 555);
            const choice = hash(cell ^ 22132), yaw = hash(cell ^ 551) * Math.PI * 2;
            const y = height(x, z);
            const slope = Math.hypot(height(x + 1, z) - y, height(x, z + 1) - y);
            const near = Math.hypot(x - 4, z + 5) < 22;
            const cabinClear = name === 'snow' && Math.hypot(x - snowCabin[0], z - snowCabin[1]) < 7;
            const chance = name === 'tropical' ? .25 + .76 * smooth((habitat - .28) / .43) : .34 + .52 * habitat;
            if (!cabinClear && name !== 'desert' && slope < .95 && choice < chance &&
                (name !== 'snow' || (y < 6.1 && habitat > .46))) {
                const species = selectCanopy(name, habitat, choice);
                const size = (name === 'bamboo' ? 5.8 : name === 'snow' ? 4.4 : 5.4) + hash(cell ^ 822) * 5.1;
                copyPrototype(objects, data.plants[species], x, y - .09, z, size, yaw, .87 + .24 * habitat, !near);
                if (species === 'palm') {
                    const bendX = size * .052, bendZ = -size * .011;
                    crowns.push([x + Math.cos(yaw) * bendX + Math.sin(yaw) * bendZ,
                                 y - .09 + size * .874,
                                 z - Math.sin(yaw) * bendX + Math.cos(yaw) * bendZ]);
                }
                plants++;
            }
            if (!cabinClear && near && (name === 'forest' || name === 'tropical' || name === 'bamboo') &&
                habitat > .36 && hash(cell ^ 219) > .33) {
                const px = x - 1.8 + hash(cell ^ 667) * 3, pz = z + 1.3;
                const kind = secondaryVegetation(name, seed, px, pz);
                copyPrototype(objects, data.plants[kind], px, height(px, pz) - .03, pz,
                              .58 + habitat * .63, yaw, .85 + .19 * habitat, false);
                plants++;
            }
            if (!cabinClear && hash(cell ^ 119) > (name === 'desert' ? .75 : .67)) {
                const sx = .55 + hash(cell ^ 971) * (name === 'snow' ? 3.5 : 1.8);
                const px = x + spacing * .27, pz = z - spacing * .16;
                const sy = sx * (.43 + .35 * hash(cell ^ 72));
                const tint = .70 + .35 * hash(cell ^ 901);
                objects.push({mesh:data.meshIds.rock,instance:[
                    ...matrix(px, height(px, pz) - sy * .06, pz, sx, sy, sx * .86, yaw),
                    tint, tint*.98, tint*.91, 1, .84, 0, 31, 0
                ]});
                rocks++;
            }
        }
    if (name === 'snow')
        copyPrototype(objects, data.snowCabin, snowCabin[0], height(...snowCabin), snowCabin[1], .91, -.18, 1, false);
    const vines = name === 'tropical' ? connectVines(objects, data.meshIds.vine, crowns, seed) : null;
    return {height, terrain, objects, plants, rocks, vines};
}
