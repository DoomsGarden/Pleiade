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
NAMES = (
    "Pino · aguja", "Pícea y enebro · aguja", "Helecho · pinna", "Abedul · hoja",
    "Campanilla · pétalo", "Corteza basal", "Pino · corteza alta", "Madera aserrada",
    "Mampostería", "Arena", "Nieve", "Piedra", "Suelo orgánico", "Bambú · caña",
    "Pino · fascículos lejanos", "Conífera · ramilla lejana",
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


def needle_spray(u, v, pine):
    coverage = curve_mask(u, v, (.5, .018), (.5, .96), .012, .015)
    stems = coverage.copy()
    for node in range(9 if pine else 12):
        y = .10 + node * (.080 if pine else .066)
        for side in (-1, 1):
            base = (.5 + .015 * np.sin(y * np.pi), y + .012 * side)
            length = (.305 if pine else .25) * (1 - .48 * y)
            if pine:
                for leaf in (-1, 1):
                    end = (.5 + side * (length * (1 + .08 * leaf)), base[1] + .16 + leaf * .029)
                    coverage = np.maximum(coverage, curve_mask(u, v, base, end, .0060, side * .025))
            else:
                tip = (.5 + side * length, base[1] + .15)
                twig = curve_mask(u, v, base, tip, .005, side * .009)
                stems = np.maximum(stems, twig)
                for k in range(2, 8):
                    t = k / 8
                    p = (base[0] + (tip[0] - base[0]) * t, base[1] + (tip[1] - base[1]) * t)
                    end = (p[0] + side * (.048 - .019 * t), p[1] + (.036 if k % 2 else -.041))
                    coverage = np.maximum(coverage, curve_mask(u, v, p, end, .006, side * .006))
    return np.maximum(coverage, stems), stems


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
            color = mix((.09, .22, .054), (.53, .68, .22), tint)
            height = .2 * vein + .10 * ribs + .26 * (1 - x * x) + fine * .10
            rough = .55 + broad * .32
        elif layer == 4:
            fibres = .5 + .5 * np.sin((u + (broad - .5) * .04) * (2 * np.pi * 13))
            color = mix((.27, .22, .56), (.66, .57, .87), .5 * v + .24 * fibres + .18 * grain)
            height = fibres * .16 + broad * .31 + fine * .04
            rough = .47 + grain * .34
        elif layer in (5, 6):
            warped = u + .027 * np.sin(v * np.pi * 4) + (broad - .5) * .043
            stripes = .5 + .5 * np.sin(warped * 2 * np.pi * 9)
            cracks = (.5 + .5 * np.sin((v + broad * .26) * 2 * np.pi * (6 if layer == 6 else 2))) ** 9
            ridge = smooth(.14, .85, stripes) * (1 - cracks * (.80 if layer == 6 else .26))
            dark, light = {
                5: ((.18, .14, .105), (.52, .44, .36)),
                6: ((.23, .13, .095), (.63, .40, .265)),
            }[layer]
            color = mix(dark, light, .24 * broad + .16 * fine + ridge * .56)
            height = ridge * .37 + fine * .20 + broad * .13
            rough = .79 + grain * .18
        elif layer == 7:
            fibres = blender_noise(89, .31, 16.2, along_grain=5)
            growth = blender_noise(29, .49, 27.7, along_grain=7)
            streaks = smooth(.23, .76, .65 * growth + .35 * fibres)
            color = mix((.265, .177, .113), (.57, .43, .303), .70 * streaks + .30 * grain)
            height = .16 * fibres + .084 * growth + .058 * fine
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
            ripple = .5 + .5 * np.sin((v * 13 + u * 2 + .27 * np.sin(u * np.pi * 2) + .26 * broad) * np.pi * 2)
            color = mix((.56, .385, .235), (.83, .685, .46), .56 * grain + .40 * ripple)
            height = ripple * .16 + fine * .125 + speckle * .05
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
        else:
            alpha, stem = needle_spray(u, v, layer == 14)
            color = mix((.11, .24, .10), (.42, .55, .23), grain * .6 + v * .31)
            color = mix(color, (.31, .21, .109), stem * .87)
            height = broad * .21 + fine * .08 + stem * .13
            rough = .61 + .25 * broad
        slope_x = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * (2.7 if layer in (5, 6, 7, 8, 11) else 1.55)
        slope_y = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * (2.7 if layer in (5, 6, 7, 8, 11) else 1.55)
        normal = np.stack((-slope_x, -slope_y, np.ones_like(u)), axis=-1)
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
        cutout = index in (2, 3, 14, 15)
        position = ((index % 4 - 1.5) * 2.5, (1.5 - index // 4) * 2.3, 0)
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
    camera.data.ortho_scale = 10.6
    scene = bpy.context.scene
    scene.camera = camera
    scene.render.resolution_x = 1100
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
