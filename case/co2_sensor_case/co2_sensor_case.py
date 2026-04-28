"""
Zigbee CO2 Sensor Enclosure — Fusion 360 Script
════════════════════════════════════════════════
Если ты здесь — значит уже добавил скрипт. Просто нажми Run.

Если после Run видишь частично нарисованный объект и ошибку:
  1. Ctrl+Z несколько раз (или File → New Design)
  2. Shift+S → убери старый скрипт (-) → добавь папку снова (+)
  3. Run
"""

import adsk.core
import adsk.fusion
import traceback

# ══════════════════════════════════════════════════════════════════════════════
#  ПАРАМЕТРЫ
# ══════════════════════════════════════════════════════════════════════════════
WALL      = 2.0
R_CORNER  = 4.0
TOLERANCE = 0.3

BAT_L, BAT_W, BAT_H = 35.0, 20.0, 5.0
ESP_L, ESP_W, ESP_H = 26.15, 17.5, 4.0
SCD_L, SCD_W, SCD_H = 21.6,  13.4, 5.0

SHELF_H    = 1.2
TOP_CLR    = 2.0
USB_HOLE_W = 10.0
USB_HOLE_H =  4.0

LID_TOP_T  = 2.0
LID_PEG_H  = 4.0
LID_FIT    = 0.4

VENT_L     = 14.0
VENT_W     =  1.6
VENT_COUNT =  5
VENT_PITCH =  3.5

# ══════════════════════════════════════════════════════════════════════════════
#  ПРОИЗВОДНЫЕ
# ══════════════════════════════════════════════════════════════════════════════
IN_L   = max(BAT_L, ESP_L, SCD_L) + 2 * TOLERANCE + 2.0
IN_W   = max(BAT_W, ESP_W, SCD_W) + 2 * TOLERANCE + 2.0
IN_H   = BAT_H + SHELF_H + ESP_H + SHELF_H + SCD_H + TOP_CLR

BODY_L = IN_L + 2 * WALL
BODY_W = IN_W + 2 * WALL
BODY_H = IN_H + WALL

Z_SHELF1 = WALL + BAT_H
Z_SHELF2 = Z_SHELF1 + SHELF_H + ESP_H
Z_USB    = WALL + BAT_H + SHELF_H + 0.5
LID_OFF  = BODY_L + 12.0

# ══════════════════════════════════════════════════════════════════════════════
#  КОНСТАНТЫ API
# ══════════════════════════════════════════════════════════════════════════════
OP_NEW  = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
OP_JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
OP_CUT  = adsk.fusion.FeatureOperations.CutFeatureOperation
DIR_POS = adsk.fusion.ExtentDirections.PositiveExtentDirection
DIR_NEG = adsk.fusion.ExtentDirections.NegativeExtentDirection


# ══════════════════════════════════════════════════════════════════════════════
#  УТИЛИТЫ
# ══════════════════════════════════════════════════════════════════════════════
def cm(v):      return v / 10.0
def val(v):     return adsk.core.ValueInput.createByReal(cm(v))
def pt(x, y):   return adsk.core.Point3D.create(cm(x), cm(y), 0)

def rect(sk, x0, y0, x1, y1):
    sk.sketchCurves.sketchLines.addTwoPointRectangle(pt(x0, y0), pt(x1, y1))

def mk_plane(comp, base, offset_mm):
    inp = comp.constructionPlanes.createInput()
    inp.setByOffset(base, val(offset_mm))
    return comp.constructionPlanes.add(inp)

def do_ext(comp, profile, op, dist_mm=None, direction=DIR_POS, through_all=False):
    """Универсальное выдавливание через setOneSideExtent"""
    inp = comp.features.extrudeFeatures.createInput(profile, op)
    if through_all:
        extent = adsk.fusion.ThroughAllExtentDefinition.create()
    else:
        extent = adsk.fusion.DistanceExtentDefinition.create(val(abs(dist_mm)))
    inp.setOneSideExtent(extent, direction)
    return comp.features.extrudeFeatures.add(inp)

def largest_profile(sk):
    """Профиль с наибольшей bounding box (рамка полки)"""
    best_p, best_a = None, -1.0
    for i in range(sk.profiles.count):
        p  = sk.profiles.item(i)
        bb = p.boundingBox
        a  = (bb.maxPoint.x - bb.minPoint.x) * (bb.maxPoint.y - bb.minPoint.y)
        if a > best_a:
            best_a, best_p = a, p
    return best_p

def smallest_n_profiles(sk, n):
    """N профилей с наименьшей bounding box (вентпрорези)"""
    lst = []
    for i in range(sk.profiles.count):
        p  = sk.profiles.item(i)
        bb = p.boundingBox
        a  = (bb.maxPoint.x - bb.minPoint.x) * (bb.maxPoint.y - bb.minPoint.y)
        lst.append((a, p))
    lst.sort(key=lambda x: x[0])
    return [p for _, p in lst[:n]]

def fillet_vertical(comp, solid, r_mm):
    """Скруглить вертикальные рёбра (запускать ПОСЛЕ всех операций)"""
    ec = adsk.core.ObjectCollection.create()
    for e in solid.edges:
        s = e.startVertex.geometry
        v = e.endVertex.geometry
        if abs(s.x - v.x) < 1e-4 and abs(s.y - v.y) < 1e-4 and abs(s.z - v.z) > 1e-4:
            ec.add(e)
    if ec.count == 0:
        return
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(ec, val(r_mm), True)
    comp.features.filletFeatures.add(fi)


# ══════════════════════════════════════════════════════════════════════════════
#  КОРПУС
# ══════════════════════════════════════════════════════════════════════════════
def create_body(comp):
    xy = comp.xYConstructionPlane
    yz = comp.yZConstructionPlane

    # 1. Внешний блок
    sk1 = comp.sketches.add(xy)
    rect(sk1, 0, 0, BODY_L, BODY_W)
    feat  = do_ext(comp, sk1.profiles.item(0), OP_NEW, BODY_H)
    solid = feat.bodies.item(0)
    solid.name = "Body"

    # 2. Внутренний карман
    #    construction plane на высоте WALL → режем через всё вверх
    #    (ThroughAll надёжнее фиксированного расстояния)
    pl_bot = mk_plane(comp, xy, WALL)
    sk2    = comp.sketches.add(pl_bot)
    rect(sk2, WALL, WALL, WALL + IN_L, WALL + IN_W)
    do_ext(comp, sk2.profiles.item(0), OP_CUT, through_all=True)

    # 3. Отверстие USB-C (плоскость YZ = стенка X=0)
    #    На YZ: горизонталь = Y, вертикаль = Z
    sk3 = comp.sketches.add(yz)
    y0  = (BODY_W - USB_HOLE_W) / 2.0
    rect(sk3, y0, Z_USB, y0 + USB_HOLE_W, Z_USB + USB_HOLE_H)
    do_ext(comp, sk3.profiles.item(0), OP_CUT, through_all=True)

    # 4. Полка 1 (над LiPo)
    #    ovr=0.1 мм: полка чуть заходит в стенки → JOIN гарантированно цепляется
    ovr = 0.1
    pl1  = mk_plane(comp, xy, Z_SHELF1)
    sk4  = comp.sketches.add(pl1)
    rect(sk4, WALL - ovr, WALL - ovr, WALL + IN_L + ovr, WALL + IN_W + ovr)
    rect(sk4, WALL + IN_L/2 - 6, WALL + IN_W/2 - 4,
              WALL + IN_L/2 + 6, WALL + IN_W/2 + 4)
    do_ext(comp, largest_profile(sk4), OP_JOIN, SHELF_H)

    # 5. Полка 2 (над ESP)
    pl2 = mk_plane(comp, xy, Z_SHELF2)
    sk5 = comp.sketches.add(pl2)
    rect(sk5, WALL - ovr, WALL - ovr, WALL + IN_L + ovr, WALL + IN_W + ovr)
    rect(sk5, WALL + IN_L/2 - 5, WALL + IN_W/2 - 3,
              WALL + IN_L/2 + 5, WALL + IN_W/2 + 3)
    do_ext(comp, largest_profile(sk5), OP_JOIN, SHELF_H)

    # 6. Скругление — ПОСЛЕДНИМ (после всех cuts и joins)
    fillet_vertical(comp, solid, R_CORNER)

    return solid


# ══════════════════════════════════════════════════════════════════════════════
#  КРЫШКА
# ══════════════════════════════════════════════════════════════════════════════
def create_lid(comp):
    xy  = comp.xYConstructionPlane
    x0  = LID_OFF
    cx  = x0 + BODY_L / 2.0
    cy  = BODY_W / 2.0

    # 1. Пег вниз (создаёт новое тело)
    px0, py0 = x0 + WALL + LID_FIT, WALL + LID_FIT
    px1, py1 = x0 + WALL + IN_L - LID_FIT, WALL + IN_W - LID_FIT
    sk1   = comp.sketches.add(xy)
    rect(sk1, px0, py0, px1, py1)
    feat  = do_ext(comp, sk1.profiles.item(0), OP_NEW, LID_PEG_H, DIR_NEG)
    solid = feat.bodies.item(0)
    solid.name = "Lid"

    # 2. Верхняя пластина вверх (JOIN с пегом — общая грань Z=0)
    sk2 = comp.sketches.add(xy)
    rect(sk2, x0, 0, x0 + BODY_L, BODY_W)
    do_ext(comp, sk2.profiles.item(0), OP_JOIN, LID_TOP_T)

    # 3. Вентиляционные прорези (режем вверх через пластину)
    sk3  = comp.sketches.add(xy)
    span = (VENT_COUNT - 1) * VENT_PITCH
    for i in range(VENT_COUNT):
        yc = cy - span / 2.0 + i * VENT_PITCH
        rect(sk3, cx - VENT_L/2, yc - VENT_W/2,
                  cx + VENT_L/2, yc + VENT_W/2)
    for p in smallest_n_profiles(sk3, VENT_COUNT):
        do_ext(comp, p, OP_CUT, LID_TOP_T + 0.1)

    return solid


# ══════════════════════════════════════════════════════════════════════════════
#  ЗАПУСК
# ══════════════════════════════════════════════════════════════════════════════
def run(context):
    ui = None
    try:
        app    = adsk.core.Application.get()
        ui     = app.userInterface
        design = adsk.fusion.Design.cast(app.activeProduct)
        root   = design.rootComponent

        create_body(root)
        create_lid(root)

        ui.messageBox(
            "Готово!\n\n"
            f"Корпус:  {BODY_L:.1f} × {BODY_W:.1f} × {BODY_H:.1f} мм\n"
            f"Крышка:  {BODY_L:.1f} × {BODY_W:.1f} мм  (пег {LID_PEG_H:.0f} мм)\n\n"
            "Для печати — ПКМ по телу → Save as Mesh → STL\n"
            "Крышку печатать перевёрнутой (плоской стороной на стол)."
        )

    except Exception:
        if ui:
            ui.messageBox("Ошибка:\n" + traceback.format_exc())
