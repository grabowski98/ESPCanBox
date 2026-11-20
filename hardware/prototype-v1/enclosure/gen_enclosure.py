# Enclosure generator for the prototype board (100 x 77 mm PCB), for 3D printing.
# Output: enclosure_bottom.stl, enclosure_top.stl, preview.png.
#
# Run (from hardware/prototype-v1/enclosure):
#   uv run --with manifold3d --with numpy --with matplotlib python gen_enclosure.py
#
# Coordinates: X and Y as in the top view of the board in KiCad, origin in the top left corner
# of the board, Y down. In the 3D model the Y axis is flipped (function by()) so the enclosure
# does not come out mirrored.
import struct

import numpy as np
from manifold3d import Manifold

# ---------- board: from centralka_proto.kicad_pcb ----------
PCB_X, PCB_Y, PCB_T = 100.0, 77.0, 2.0   # thickness measured with calipers: just under 2 mm
HOLES = [(22.0, 3.5), (97.0, 3.2), (96.5, 73.5)]   # M3 holes: H1, H2, H3
SUPPORT = (2.5, 74.5)                               # support without screw (bottom left corner)

# ---------- heights: measured with calipers on the built prototype ----------
UNDER = 14.5      # below the board: tallest capacitor is 13 mm (measured), 1.5 mm margin
ABOVE = 24.5      # above the board: green terminal on the module is 22 mm (measured), 2.5 mm margin
SPLIT = 5.0       # split bottom / top: this far above the top surface of the board

# ---------- walls and clearances ----------
WALL, FLOOR, ROOF = 2.4, 2.0, 2.0
GAP = 1.5         # clearance board - wall
TONGUE_T, TONGUE_H, TONGUE_GAP = 1.2, 2.0, 0.2   # tongue of the top part that enters the bottom part

# ---------- cut-outs (position along the board edge, height above the board top) ----------
# left wall (X = 0): wires
NOTCH_J5 = (1.0, 23.0)      # Y from-to: terminals CANH, CANL, GND, +12V
NOTCH_J4 = (36.0, 73.0)     # Y from-to: wires to the radio (and speaker)
NOTCH_Z = (1.5, 9.0)        # above the board from-to
# bottom wall (Y = 77): USB sockets of the ESP32 module
USB_X = (70.0, 97.0)
USB_Z = (SPLIT, 17.0)

SEG = 48


def by(y):
    """Y of the board drawing -> Y of the model."""
    return PCB_Y - y


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl(x, y, z0, z1, r0, r1=None):
    return Manifold.cylinder(z1 - z0, r0, r0 if r1 is None else r1, SEG).translate([x, y, z0])


# Z levels: floor from -FLOOR to 0
Z_PCB = UNDER                 # bottom of the board
Z_TOP = UNDER + PCB_T         # top of the board
Z_SPLIT = Z_TOP + SPLIT
Z_CEIL = Z_TOP + ABOVE
# inside and outside in plan view
IX0, IX1, IY0, IY1 = -GAP, PCB_X + GAP, -GAP, PCB_Y + GAP
OX0, OX1, OY0, OY1 = IX0 - WALL, IX1 + WALL, IY0 - WALL, IY1 + WALL
EAR_R = 5.0
EAR_ARM = 14.0    # length along which an ear of the top part joins the wall (both ways from the corner)
EAR_RISE = 14.0   # height of the reinforcement above the split (4 mm flange + slope)
EARS = [(OX0 - 2, OY0 - 2), (OX1 + 2, OY0 - 2), (OX0 - 2, OY1 + 2), (OX1 + 2, OY1 + 2)]


def notches():
    """Cut-outs shared by both parts (they cross the split)."""
    n = Manifold()
    for y0, y1 in (NOTCH_J5, NOTCH_J4):
        n += box(OX0 - 1, IX0 + TONGUE_GAP + TONGUE_T + 0.5, by(y1), by(y0),
                 Z_TOP + NOTCH_Z[0], Z_TOP + NOTCH_Z[1])
    n += box(USB_X[0], USB_X[1], OY0 - 1, IY0 + TONGUE_GAP + TONGUE_T + 0.5,
             Z_TOP + USB_Z[0], Z_TOP + USB_Z[1])
    return n


def base():
    b = box(OX0, OX1, OY0, OY1, -FLOOR, Z_SPLIT) - box(IX0, IX1, IY0, IY1, 0, Z_SPLIT + 1)
    for x, y in EARS:                                   # ears for the screws joining the halves
        b += cyl(x, y, -FLOOR, Z_SPLIT, EAR_R)
    for x, y in HOLES:                                  # posts under the board
        b += cyl(x, by(y), 0, 3, 5.0, 3.0) + cyl(x, by(y), 0, Z_PCB, 3.0)
    b += cyl(SUPPORT[0], by(SUPPORT[1]), 0, Z_PCB, 2.0)
    for x, y in HOLES:                                  # 2.8 mm hole: the M3 screw cuts its own thread in the plastic
        b -= cyl(x, by(y), Z_PCB - 10, Z_PCB + 1, 1.4)
    for x, y in EARS:
        b -= cyl(x, y, Z_SPLIT - 12, Z_SPLIT + 1, 1.4)
    return b - notches()


def cap():
    c = box(OX0, OX1, OY0, OY1, Z_SPLIT, Z_CEIL + ROOF) - box(IX0, IX1, IY0, IY1, Z_SPLIT - 1, Z_CEIL)
    # tongue: frame entering TONGUE_H into the bottom part, keeps the halves aligned
    g = TONGUE_GAP
    inner = box(IX0 + g + TONGUE_T, IX1 - g - TONGUE_T, IY0 + g + TONGUE_T, IY1 - g - TONGUE_T,
                Z_SPLIT - TONGUE_H - 1, Z_SPLIT + 4)
    c += box(IX0 + g, IX1 - g, IY0 + g, IY1 - g, Z_SPLIT - TONGUE_H, Z_SPLIT) - inner   # part entering the bottom
    c += box(IX0 - 0.1, IX1 + 0.1, IY0 - 0.1, IY1 + 0.1, Z_SPLIT, Z_SPLIT + 3) - inner   # joint with the wall
    # Ears of the top part (a plain ring had too little contact with the wall).
    # Each ear is a solid corner: 4 mm flange under the screw head joined with the walls
    # over EAR_ARM both ways and a slope up to EAR_RISE (prints without supports).
    cavity = box(IX0, IX1, IY0, IY1, Z_SPLIT - 1, Z_CEIL)
    for x, y in EARS:
        sx = 1 if x < 0 else -1                 # direction from the corner along the X wall
        sy = 1 if y < 0 else -1
        cx, cy = (OX0 if sx > 0 else OX1), (OY0 if sy > 0 else OY1)   # outer corner of the enclosure
        ax = sorted((cx, cx + sx * EAR_ARM)); wx = sorted((cx, cx + sx * WALL))
        ay = sorted((cy, cy + sy * EAR_ARM)); wy = sorted((cy, cy + sy * WALL))
        g = cyl(x, y, Z_SPLIT, Z_SPLIT + 4, EAR_R)                              # flange of the ear
        g += box(ax[0], ax[1], wy[0], wy[1], Z_SPLIT, Z_SPLIT + EAR_RISE)       # strip in the wall along X
        g += box(wx[0], wx[1], ay[0], ay[1], Z_SPLIT, Z_SPLIT + EAR_RISE)       # strip in the wall along Y
        c += g.hull() - cavity
    for x, y in EARS:
        c -= cyl(x, y, Z_SPLIT - 1, Z_SPLIT + 5, 1.7)               # 3.4 mm through hole
        c -= cyl(x, y, Z_SPLIT + 4, Z_SPLIT + EAR_RISE + 1, 3.4)    # room for the screw head
    return c - notches()


def write_stl(m, path):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    t = np.asarray(mesh.tri_verts)
    with open(path, "wb") as f:
        f.write(b"prototype-v1 enclosure".ljust(80, b" "))
        f.write(struct.pack("<I", len(t)))
        for a, b_, c_ in t:
            p0, p1, p2 = v[a], v[b_], v[c_]
            n = np.cross(p1 - p0, p2 - p0)
            ln = np.linalg.norm(n)
            n = n / ln if ln else n
            f.write(struct.pack("<12fH", *n, *p0, *p1, *p2, 0))
    return v, t


def preview(parts, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(14, 7))
    for i, (name, v, t, col) in enumerate(parts):
        ax = fig.add_subplot(1, len(parts), i + 1, projection="3d")
        tri = v[t]
        nrm = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
        shade = 0.45 + 0.55 * np.clip(nrm @ np.array([0.3, -0.5, 0.8]), 0, 1)
        pc = Poly3DCollection(tri, facecolors=np.outer(shade, col), edgecolors="none")
        ax.add_collection3d(pc)
        lo, hi = v.min(0), v.max(0)
        ax.set_xlim(lo[0], hi[0]); ax.set_ylim(lo[1], hi[1]); ax.set_zlim(lo[2], lo[2] + (hi[0] - lo[0]))
        ax.set_box_aspect((hi[0] - lo[0], hi[1] - lo[1], hi[0] - lo[0]))
        ax.view_init(elev=38, azim=-125)
        ax.set_title(name)
        ax.set_axis_off()
    fig.tight_layout()
    fig.savefig(path, dpi=110)


if __name__ == "__main__":
    b, c = base(), cap()
    vb, tb = write_stl(b, "enclosure_bottom.stl")
    # the top part is saved roof down: that is how it is printed
    c_print = c.rotate([180, 0, 0])
    c_print = c_print.translate([0, 0, -c_print.bounding_box()[2]])
    vc, tc = write_stl(c_print, "enclosure_top.stl")
    vca, tca = (np.asarray(c.to_mesh().vert_properties)[:, :3], np.asarray(c.to_mesh().tri_verts))
    preview([("bottom (board inside)", vb, tb, np.array([0.55, 0.7, 0.9])),
             ("top (as assembled)", vca, tca, np.array([0.9, 0.75, 0.5]))], "preview.png")
    for name, m in (("bottom", b), ("top", c)):
        bb = m.bounding_box()
        print(f"{name}: {bb[3]-bb[0]:.1f} x {bb[4]-bb[1]:.1f} x {bb[5]-bb[2]:.1f} mm, "
              f"volume {m.volume()/1000:.1f} cm3, solids: {len(m.decompose())}")
    print(f"outside without ears: {OX1-OX0:.1f} x {OY1-OY0:.1f} x {Z_CEIL+ROOF+FLOOR:.1f} mm")
