"""Create Pléiade's editable Blender materials and export their game texture layers.

blender -b -t 2 -P tools/blender/bake_surfaces.py -- dist/assets assets/blender/pleiade-surfaces.blend
"""

import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector, noise

SIZE = 256
SCAN = Path(__file__).resolve().parents[2] / "assets/blender/scans"
NAMES = (
    "Pino · aguja", "Pícea y enebro · aguja", "Helecho · pinna", "Abedul · hoja",
    "Campanilla · pétalo", "Corteza basal", "Pino · corteza alta", "Madera aserrada",
    "Mampostería", "Arena", "Nieve", "Piedra", "Suelo orgánico", "Bambú · caña",
    "Pino · fascículos lejanos", "Conífera · ramilla lejana", "Abedul · corteza", "Palmera · cicatrices foliares",
    "Caducifolia · rama lejana", "Helecho · fronde lejana",
)


def smooth(low, high, value):
    t = np.clip((value - low) / (high - low), 0, 1)
    return t * t * (3 - 2 * t)


def mix(a, b, factor):
    t = np.asarray(factor)[..., None]
    return np.asarray(a) * (1 - t) + np.asarray(b) * t


def blender_noise(grid, extent, offset, along_grain=None):
    # Sample Blender's noise on a periodic lattice, then interpolate at texture resolution.
    columns = along_grain or grid
    points = np.array([[noise.noise(Vector((x * extent, y * extent, offset)), noise_basis="PERLIN_ORIGINAL")
                        for x in range(columns)] for y in range(grid)], dtype=np.float32)
    def samples(count):
        coordinate = np.arange(SIZE) * count / SIZE
        first = coordinate.astype(int)
        return first, (first + 1) % count, smooth(0, 1, coordinate - first)
    x0, x1, wx = samples(columns)
    y0, y1, wy = samples(grid)
    upper = points[np.ix_(y0, x0)] * (1 - wx) + points[np.ix_(y0, x1)] * wx
    lower = points[np.ix_(y1, x0)] * (1 - wx) + points[np.ix_(y1, x1)] * wx
    return np.clip(.5 + (upper * (1 - wy[:, None]) + lower * wy[:, None]) * .85, 0, 1)


def curve_mask(u, v, start, end, width, bend=0):
    ax, ay = start
    bx, by = end
    dx, dy = bx - ax, by - ay
    span = math.hypot(dx, dy)
    t = ((u - ax) * dx + (v - ay) * dy) / (span * span)
    distance = ((u - ax) * -dy + (v - ay) * dx) / span - bend * np.sin(np.pi * t)
    radius = width * (1 - .82 * np.clip(t, 0, 1))
    return (1 - smooth(radius - .002, radius + .003, np.abs(distance))) * smooth(-.003, .004, t) * (1 - smooth(.98, 1.01, t))


def bark_fissures(u, v, layer, noise_map):
    columns, rows = (15, 6) if layer == 5 else (12, 9)
    rng = np.random.default_rng(824 + layer)
    u = u + (noise_map - .5) * .032
    v = v + (noise_map - .5) * .060
    first, second = np.full_like(u, 100.), np.full_like(u, 100.)
    for row in range(rows):
        for column in range(columns):
            cx = (column + rng.uniform(.13, .87)) / columns
            cy = (row + rng.uniform(.12, .88)) / rows
            dx = ((u - cx + .5) % 1 - .5) * columns
            dy = ((v - cy + .5) % 1 - .5) * rows
            d = np.hypot(dx, dy)
            second = np.minimum(second, np.maximum(first, d))
            first = np.minimum(first, d)
    return 1 - smooth(.013, .18 if layer == 5 else .10, second - first)


def needle_spray(u, v, pine):
    coverage = curve_mask(u, v, (.5, .018), (.5, .96), .012, .015)
    stems = coverage.copy()
    for node in range(10 if pine else 13):
        y = .099 + node * (.079 if pine else .061) + .015 * np.sin(node * 4.67)
        for side in (-1, 1):
            base = (.5 + .015 * np.sin(y * np.pi), y + .015 * side * np.sin(node * 2.72))
            wavering = 1 + .23 * np.sin(node * 3.3 + side * 2.2)
            length = (.316 if pine else .26) * (1 - .53 * y) * wavering
            if pine:
                for leaf in (-1.5, -.5, .5, 1.5):
                    end = (.5 + side * (length * (1 + .086 * leaf)), base[1] + .124 * wavering + leaf * .022)
                    for spray in (-.75, .6):
                        start=(base[0]+side*length*.18,base[1]+spray*.014)
                        tip=(end[0]+side*.014*spray,end[1]+spray*.046)
                        coverage = np.maximum(coverage, curve_mask(u, v, start, tip, .0101, side * .023))
            else:
                tip = (.5 + side * length, base[1] + .15)
                twig = curve_mask(u, v, base, tip, .005, side * .009)
                stems = np.maximum(stems, twig)
                for k in range(2, 14):
                    t = k / 14
                    p = (base[0] + (tip[0] - base[0]) * t, base[1] + (tip[1] - base[1]) * t)
                    end = (p[0] + side * (.055 - .029 * t), p[1] + (.037 if k % 2 else -.039) * wavering)
                    coverage = np.maximum(coverage, curve_mask(u, v, p, end, .0093, side * .004))
    return np.maximum(coverage, stems), stems


def scan_map(name):
    image = bpy.data.images.load(str(SCAN / name), check_existing=False)
    image.colorspace_settings.name = "Non-Color"
    image.scale(SIZE, SIZE)
    pixels = np.asarray(image.pixels[:], dtype=np.float32).reshape(SIZE, SIZE, 4)
    bpy.data.images.remove(image)
    return pixels


def broad_spray(u, v, fern):
    center=curve_mask(u,v,(.50,.02),(.50,.98),.008,.014)
    coverage=center.copy()
    for j in range(13 if fern else 8):
        y=.13+j*(.061 if fern else .095)
        for side in (-1,1):
            angle=side*(.66 + .095*np.sin(j*4.1+side))
            length=(.38 if fern else .35) * np.sin(np.pi*y)**(.7 if fern else .53)
            x0=.50+side*length*.49+.013*np.sin(j*1.32)
            y0=y+length*.23+side*.008
            dx,dy=u-x0,v-y0
            along=dx*np.sin(angle)+dy*np.cos(angle)
            across=dx*np.cos(angle)-dy*np.sin(angle)
            if fern:
                r=np.hypot(along/(length*.76),across/(.031+.018*np.sin(np.pi*y)))
                coverage=np.maximum(coverage,1-smooth(.83,1.0,r+.058*np.cos(along*455)))
            else:
                for offset in (-2,-1,0,1,2):
                    r=np.hypot((along+offset*.043)/(length*.222),(across+offset*.024+.019*np.sin(offset*3.7+j))/(.022+.010*np.sin(j*3.7+side)**2))
                    coverage=np.maximum(coverage,1-smooth(.86,1.01,r))
            petiole=curve_mask(u,v,(.5,y),(x0,y0),.004,side*.003)
            coverage=np.maximum(coverage,petiole)
    return coverage,center


def author_layers():
    axis = (np.arange(SIZE) + .5) / SIZE
    u, v = np.meshgrid(axis, axis)
    rng = np.random.default_rng(73)
    speckle = rng.random((SIZE, SIZE))
    broad = blender_noise(11, .37, 4.2)
    fine = blender_noise(51, .63, 11.8)
    grain = broad * .47 + fine * .44 + speckle * .09
    leaf_noise = .74 * broad + .26 * fine
    x = np.abs(u - .5) * 2
    vein = np.exp(-x * 45)
    layers = []
    pine_color = scan_map("pine_bark-Diffuse.jpg")[..., :3]
    pine_normal = scan_map("pine_bark-nor_gl.jpg")[..., :3]*2-1
    pine_roughness = scan_map("pine_bark-Rough.jpg")[..., 0]

    for layer in range(len(NAMES)):
        alpha = np.ones_like(u)
        if layer < 2:
            line = .5 + .5 * np.cos(u * 2 * np.pi)
            tint = np.clip(.30 * leaf_noise + .45 * v + .23 * line, 0, 1)
            color = mix((.115, .22, .17), (.43, .56, .33), tint) if layer == 0 else mix((.065, .15, .07), (.36, .47, .19), tint)
            height = line * .39 + fine * .095 + .075 * np.cos(u * np.pi * 14)
            rough = .53 + fine * .25
        elif layer in (2, 3):
            waves = .5 + .5 * np.sin(v * np.pi * (37 if layer == 2 else 25))
            shape = np.sin(np.pi * v) ** (.62 if layer == 2 else .74)
            width = shape * (.91 - waves * (.075 if layer == 2 else .115))
            alpha = 1 - smooth(width - .012, width + .009, x)
            ribs = np.exp(-np.abs(np.sin(np.pi * (v * (13 if layer == 2 else 8) - x * 1.4))) * 15)
            tint = np.clip(leaf_noise * .64 + vein * .26 + ribs * .16 + v * .12, 0, 1)
            color = mix((.095, .231, .071), (.44, .59, .25), tint)
            height = .2 * vein + .10 * ribs + .26 * (1 - x * x) + fine * .10
            rough = .55 + broad * .32
        elif layer == 4:
            fibres = .5 + .5 * np.sin((u + (broad - .5) * .04) * (2 * np.pi * 13))
            color = mix((.27, .22, .56), (.66, .57, .87), .5 * v + .24 * fibres + .18 * grain)
            height = fibres * .16 + broad * .31 + fine * .04
            rough = .47 + grain * .34
        elif layer in (5, 6):
            slabs = blender_noise(12, .61, 23.1 + layer, along_grain=36)
            splinters = blender_noise(29, .58, 42.1 + layer, along_grain=109)
            block = slabs * .66 + splinters * .23 + broad * .11
            fissure = bark_fissures(u, v, layer, .72 * fine + .28 * splinters)
            scales = smooth(.39, .65, block + (fine - .5) * (.22 if layer == 6 else .10))
            dark, light = {
                5: ((.18, .14, .105), (.52, .44, .36)),
                6: ((.23, .13, .095), (.63, .40, .265)),
            }[layer]
            authored = mix(dark, light, .28 * broad + .16 * fine + scales * .43 - fissure * .20)
            scanned = pine_color if layer == 5 else pine_color*np.array((1.26, .98, .80))
            lichen = smooth(.61,.83, .62*broad+.38*fine)*(.51 if layer == 5 else .19)
            color = mix(mix(authored,scanned,.89),(.44,.457,.33),lichen)
            height = -.28 * fissure + .31 * scales + .21 * splinters + .13 * fine
            rough = .26*(.79+grain*.18) + .74*pine_roughness
        elif layer == 7:
            fibres = blender_noise(114, .46, 16.2, along_grain=13)
            growth = blender_noise(43, .51, 27.7, along_grain=9)
            knots = np.zeros_like(u)
            warped = v.copy()
            for cx,cy,r in [(.18,.21,.088),(.61,.81,.065),(.85,.43,.102)]:
                dx=(u-cx+.5)%1-.5;dy=(v-cy+.5)%1-.5
                mask=np.exp(-((dx/(r*1.8))**2+(dy/r)**2))
                warped += dy*mask*.61
                knots=np.maximum(knots,mask*.76 + mask*.17*np.cos(np.hypot(dx*.55,dy)*270))
            rings=.5+.5*np.sin(2*np.pi*(warped*31+(growth-.5)*.58))
            streaks = smooth(.23, .76, .50 * growth + .30 * fibres + .20*rings)
            color = mix((.29, .211, .15), (.61, .489, .377), .63 * streaks + .37 * grain)
            color=mix(color,(.21,.143,.106),knots*.64)
            height = .20 * fibres + .089 * growth + .07 * fine + .06*rings - knots*.09
            rough = .69 + .22 * grain
        elif layer == 8:
            courses = v * 4
            stagger = np.floor(courses) % 2 * .5
            edge = np.minimum(np.abs((u * 3 + stagger + .5) % 1 - .5), np.abs((courses + .5) % 1 - .5))
            body = smooth(.023, .070, edge + (fine - .5) * .060)
            color = mix((.32, .315, .30), mix((.255, .275, .27), (.68, .65, .58), grain), body)
            height = body * .38 + fine * .17 + broad * .11
            rough = .82 + .14 * grain
        elif layer == 9:
            phase = v * 12 - u * 2 + (blender_noise(9, .66, 25.4) - .5) * 1.76
            ripple = .5 + .5 * np.sin(phase * np.pi * 2)
            ripple_strength = smooth(.22, .68, broad) * (.55 + .45 * fine)
            color = mix((.59, .45, .30), (.83, .69, .49), .76 * grain + .12 * ripple * ripple_strength)
            height = ripple ** 2 * .12 * ripple_strength + fine * .11 + speckle * .043
            rough = .83 + fine * .13
        elif layer == 10:
            color = mix((.63, .72, .78), (.98, .985, .96), .3 + grain * .68)
            height = broad * .17 + fine * .07 + speckle * .045
            rough = .69 + grain * .22
        elif layer in (11, 12):
            flecks = smooth(.58, .83, fine * .78 + speckle * .22)
            tones = ((.20, .20, .19), (.57, .55, .51)) if layer == 11 else ((.145, .116, .071), (.43, .38, .235))
            color = mix(*tones, grain * .84 + flecks * .15)
            height = broad * .23 + fine * .22 + flecks * .19
            rough = .81 + grain * .14
        elif layer == 13:
            node = np.exp(-((np.abs((v + .5) % 1 - .5) - .018) / .027) ** 2)
            fibre = .50 + .50 * np.cos((u * 12 + fine * .22) * 2 * np.pi)
            color = mix((.18, .24, .10), (.59, .65, .32), .25 * grain + fibre * .10 + node * .50 + .19)
            height = node * .29 + fibre * .04 + grain * .14
            rough = .55 + grain * .22 + node * .10
        elif layer in (14, 15):
            alpha, stem = needle_spray(u, v, layer == 14)
            color = mix((.11, .24, .10), (.42, .55, .23), grain * .6 + v * .31)
            color = mix(color, (.31, .21, .109), stem * .87)
            height = broad * .21 + fine * .08 + stem * .13
            rough = .61 + .25 * broad
        elif layer == 16:
            scars = np.zeros_like(u)
            scar_rng = np.random.default_rng(910)
            for _ in range(78):
                cx, cy = scar_rng.random(2)
                length, thickness = scar_rng.uniform(.009, .068), scar_rng.uniform(.0036, .010)
                dx = (u - cx + .5) % 1 - .5
                dy = (v - cy + .5 + dx * scar_rng.uniform(-.04, .04)) % 1 - .5
                scar = np.exp(-((dx / length) ** 4 + (dy / thickness) ** 2))
                scars = np.maximum(scars, scar * scar_rng.uniform(.35, .96))
            peel = blender_noise(58, .48, 34.1, along_grain=10)
            color = mix((.65, .641, .599), (.94, .931, .884), .28*broad+.24*fine+.36*peel)
            tears=smooth(.56,.72,peel)*smooth(.53,.78,broad)*.55
            color = mix(color, (.129, .112, .090), np.maximum(.95*scars,tears))
            height = .064*peel-.10*scars+.04*fine
            rough = .58+.20*grain
        elif layer == 17:
            bands = np.zeros_like(u)
            for mark in (.052, .186, .295, .432, .563, .72, .814, .941):
                offset = .010 * np.sin(u * np.pi * 2 + mark * 9) + (broad - .5) * .029
                distance = (v - mark + offset + .5) % 1 - .5
                bands = np.maximum(bands, np.exp(-(distance / .008) ** 2) * (.63 + .37 * fine))
            color = mix((.30, .282, .221), (.62, .595, .50), .69 * grain + .23 * broad)
            color = mix(color, (.216, .20, .154), .69 * bands)
            height = .24 * fine - .24 * bands + .14 * broad
            rough = .78 + .17 * grain
        else:
            alpha,midrib=broad_spray(u,v,layer==19)
            color=mix((.105,.246,.063),(.46,.615,.22),.75*grain+.18*v)
            color=mix(color,(.30,.37,.16),.57*midrib)
            height=.18*broad+.11*fine+.21*midrib
            rough=.61+.22*grain
        slope_x = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * (2.7 if layer in (5, 6, 7, 8, 11) else 1.55)
        slope_y = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * (2.7 if layer in (5, 6, 7, 8, 11) else 1.55)
        normal = np.stack((-slope_x, -slope_y, np.ones_like(u)), axis=-1)
        if layer in (5,6):
            normal=normal*.22 + pine_normal*.78
        normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
        layers.append((np.dstack((color, alpha)), np.dstack((normal * .5 + .5, rough))))
    return layers


def packed_image(name, data, data_image=False):
    image = bpy.data.images.new(name, SIZE, SIZE, alpha=True)
    image.colorspace_settings.name = "Non-Color" if data_image else "sRGB"
    image.alpha_mode = "CHANNEL_PACKED"
    image.pixels.foreach_set(np.ascontiguousarray(data, dtype=np.float32).ravel())
    image.update()
    image.pack()
    return image


def make_blender_scene(layers, path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for index, (color, detail) in enumerate(layers):
        name = NAMES[index]
        material = bpy.data.materials.new(name)
        material.use_nodes = True
        nodes = material.node_tree.nodes
        links = material.node_tree.links
        shader = nodes.get("Principled BSDF")
        shader.location = (270, 40)
        nodes.get("Material Output").location = (650, 40)
        albedo = nodes.new("ShaderNodeTexImage")
        albedo.image = packed_image(name + " · color y máscara", color)
        albedo.location = (-600, 170)
        normal_rough = nodes.new("ShaderNodeTexImage")
        normal_rough.image = packed_image(name + " · normal y rugosidad", detail, True)
        normal_rough.location = (-600, -140)
        normal = nodes.new("ShaderNodeNormalMap")
        normal.location = (-110, -130)
        links.new(albedo.outputs["Color"], shader.inputs["Base Color"])
        links.new(normal_rough.outputs["Alpha"], shader.inputs["Roughness"])
        links.new(normal_rough.outputs["Color"], normal.inputs["Color"])
        links.new(normal.outputs["Normal"], shader.inputs["Normal"])
        cutout = index in (2, 3, 14, 15, 18, 19)
        position = ((index % 5 - 2) * 2.5, (1.5 - index // 5) * 2.3, 0)
        if cutout:
            links.new(albedo.outputs["Alpha"], shader.inputs["Alpha"])
            bpy.ops.mesh.primitive_plane_add(size=1.95, location=position)
        else:
            bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=.83, location=position)
            for face in bpy.context.object.data.polygons:
                face.use_smooth = True
        swatch = bpy.context.object
        swatch.name = name
        swatch.data.materials.append(material)
        bpy.ops.object.text_add(location=(position[0] - 1, position[1] - 1.12, .01))
        label = bpy.context.object
        label.data.body = name
        label.data.size = .17
    bpy.ops.object.light_add(type="AREA", location=(-3, -2, 7))
    bpy.context.object.data.energy = 1300
    bpy.context.object.data.shape = "DISK"
    bpy.context.object.data.size = 8
    bpy.ops.object.camera_add(location=(0, 0, 14))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 13
    scene = bpy.context.scene
    scene.camera = camera
    scene.render.resolution_x = 1350
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.world = bpy.data.worlds.new("Luz ambiente")
    scene.world.color = (.22, .22, .22)
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(path.resolve()), compress=True)


if __name__ == "__main__":
    output, blend = map(Path, sys.argv[sys.argv.index("--") + 1:])
    output.mkdir(parents=True, exist_ok=True)
    layers = author_layers()
    for channel, suffix in enumerate(("color", "normal")):
        with (output / f"surfaces-{suffix}.rgba").open("wb") as file:
            for layer in layers:
                file.write(np.rint(np.clip(layer[channel], 0, 1) * 255).astype(np.uint8).tobytes())
    (output / "surfaces.json").write_text(json.dumps({"size": SIZE, "layers": NAMES}, ensure_ascii=False, indent=2), encoding="utf-8")
    make_blender_scene(layers, blend)
