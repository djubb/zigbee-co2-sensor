/*
 * Zigbee CO2 Sensor Enclosure
 * ───────────────────────────────────────────────────────────────────────────
 * Components (L × W × H):
 *   LiPo 502035p        : 35.0 × 20.0 × 5.0 mm  — bottom layer
 *   ESP32-C6 SuperMini  : 26.15 × 17.5 × 4.0 mm  — middle layer (USB-C on short end)
 *   SCD-41 breakout     : 21.6 × 13.4 × 5.0 mm  — top layer (ventilated lid)
 *
 * Stacking (Z, bottom → top):
 *   wall(2) / LiPo(5) / shelf(1.2) / ESP(4) / shelf(1.2) / SCD41(5) / clearance(2) / lid
 *
 * USB-C port faces the FRONT wall (short face of the enclosure).
 * SCD-41 ventilation slots are in the lid top plate.
 *
 * How to print:
 *   1. Open in OpenSCAD (openscad.org, free)
 *   2. F6 → Render, then File → Export → STL
 *   3. Print BODY and LID separately (no supports needed — both lay flat)
 *   4. Adjust TOL if fit is too tight / too loose
 *
 * Coordinate convention:
 *   X — along enclosure LENGTH  (LiPo long axis, ~42 mm)
 *   Y — along enclosure WIDTH   (LiPo short axis, ~26 mm)
 *   Z — vertical (height,       ~21 mm)
 *   Front face (USB-C cutout) is at X = 0.
 */

$fn = 64;

// ═══════════════════════════════════════════════════════
//  PARAMETERS — tweak these to adjust the model
// ═══════════════════════════════════════════════════════

WALL      = 2.0;    // wall thickness (mm)
R_OUTER   = 4.0;   // outer corner radius — controls how "rounded" corners look
TOL       = 0.3;   // clearance added around each component (per side)

// LiPo battery
BAT_L = 35.0;  BAT_W = 20.0;  BAT_H = 5.0;

// ESP32-C6 SuperMini
ESP_L = 26.15; ESP_W = 17.5;  ESP_H = 4.0;
USB_C_EXT = 1.35;  // how far USB-C plug extends beyond PCB edge

// SCD-41 breakout module
SCD_L = 21.6;  SCD_W = 13.4;  SCD_H = 5.0;

// Internal shelves between layers
SHELF_H = 1.2;

// Air gap above SCD-41 before lid seals
TOP_CLR = 2.0;

// USB-C port cutout size
USB_CUT_W = 10.0;   // width of opening
USB_CUT_H =  4.0;   // height of opening

// Lid parameters
LID_TOP_T   = 2.0;  // lid top plate thickness
LID_RIM_H   = 5.0;  // rim height (overlaps outside of body)
LID_FIT     = 0.25; // clearance between lid rim inner and body outer (per side)

// Ventilation grid in lid (over SCD-41)
VENT_SLOT_L = 14.0;
VENT_SLOT_W =  1.6;
VENT_SLOTS  =  5;
VENT_PITCH  =  3.5;

// ═══════════════════════════════════════════════════════
//  DERIVED DIMENSIONS (do not edit unless you know why)
// ═══════════════════════════════════════════════════════

// Inner cavity — large enough for every component + tolerance
IN_L = max(BAT_L, ESP_L, SCD_L) + 2*TOL + 2.0;   // ~37.6 mm
IN_W = max(BAT_W, ESP_W, SCD_W) + 2*TOL + 2.0;   // ~22.6 mm
IN_H = BAT_H + SHELF_H + ESP_H + SHELF_H + SCD_H + TOP_CLR; // ~18.4 mm

// Body outer
BODY_L = IN_L + 2*WALL;  // ~41.6 mm
BODY_W = IN_W + 2*WALL;  // ~26.6 mm
BODY_H = IN_H + WALL;    // ~20.4 mm (closed bottom, open top)

// Z positions of layer floors (inside body, above bottom wall)
Z_SHELF1 = WALL + BAT_H;                          // top of LiPo
Z_SHELF2 = Z_SHELF1 + SHELF_H + ESP_H;            // top of ESP
Z_SCD    = Z_SHELF2 + SHELF_H;                    // SCD-41 floor
Z_USB    = Z_SHELF1 + SHELF_H + 0.5;              // center of USB-C cutout


// ═══════════════════════════════════════════════════════
//  HELPER MODULES
// ═══════════════════════════════════════════════════════

// Rounded rectangular prism aligned to origin
module rounded_box(l, w, h, r) {
    hull()
        for (dx = [r, l-r], dy = [r, w-r])
            translate([dx, dy, 0])
                cylinder(r=r, h=h);
}

// Centered slot (for ventilation openings)
module slot(l, w, h) {
    translate([-l/2, -w/2, 0])
        cube([l, w, h]);
}


// ═══════════════════════════════════════════════════════
//  BODY
// ═══════════════════════════════════════════════════════

module body() {
    difference() {
        // ── Outer shell ──────────────────────────────────────────────────────
        rounded_box(BODY_L, BODY_W, BODY_H, R_OUTER);

        // ── Main inner cavity (open top) ─────────────────────────────────────
        translate([WALL, WALL, WALL])
            cube([IN_L, IN_W, IN_H + 0.1]);

        // ── USB-C cutout (front face at X=0, centered on Y) ──────────────────
        //    The ESP is positioned against the front wall so the USB port lines up.
        translate([-0.1,
                   (BODY_W - USB_CUT_W) / 2,
                   Z_USB])
            cube([WALL + 0.2, USB_CUT_W, USB_CUT_H]);

        // ── Side ventilation slots (2 slots per long side, ESP level) ─────────
        //    Helps air circulation and cable routing
        for (side_y = [0, BODY_W - WALL])
            for (slot_x = [BODY_L * 0.35, BODY_L * 0.65])
                translate([slot_x - 1.0, side_y - 0.1, Z_SHELF1 + SHELF_H + 0.5])
                    cube([2.0, WALL + 0.2, 3.0]);
    }

    // ── Shelf 1: above LiPo, below ESP ───────────────────────────────────────
    translate([WALL, WALL, Z_SHELF1]) {
        difference() {
            cube([IN_L, IN_W, SHELF_H]);
            // Wire / connector pass-through (centered)
            translate([IN_L/2 - 6, IN_W/2 - 4, -0.1])
                cube([12, 8, SHELF_H + 0.2]);
        }
    }

    // ── Shelf 2: above ESP, below SCD-41 ─────────────────────────────────────
    translate([WALL, WALL, Z_SHELF2]) {
        difference() {
            cube([IN_L, IN_W, SHELF_H]);
            // Wire pass-through (smaller — only I2C wires)
            translate([IN_L/2 - 5, IN_W/2 - 3, -0.1])
                cube([10, 6, SHELF_H + 0.2]);
        }
    }

    // ── Locating peg for LiPo (rear corner, keeps battery from sliding) ──────
    translate([WALL + IN_L - 1.5 - TOL, WALL + IN_W - 1.5 - TOL, WALL])
        cube([1.5, 1.5, BAT_H - 0.5]);
}


// ═══════════════════════════════════════════════════════
//  LID
// ═══════════════════════════════════════════════════════

module lid() {
    // The lid sits on top of the body.
    // Origin is the OUTER BOTTOM of the lid top plate.
    // When printing: flip upside-down (rotate [180,0,0]) so the rim points up.

    cx = BODY_L / 2;
    cy = BODY_W / 2;

    difference() {
        union() {
            // Top plate
            rounded_box(BODY_L, BODY_W, LID_TOP_T, R_OUTER);

            // Rim — slides over the outside of the body
            translate([-LID_FIT, -LID_FIT, -LID_RIM_H])
                difference() {
                    rounded_box(BODY_L + 2*LID_FIT,
                                BODY_W + 2*LID_FIT,
                                LID_RIM_H,
                                R_OUTER + LID_FIT);
                    // Hollow interior of rim (body fits inside)
                    translate([WALL, WALL, -0.1])
                        cube([IN_L, IN_W, LID_RIM_H + 0.2]);
                }
        }

        // ── Ventilation grid (centered in lid, over SCD-41 area) ─────────────
        total_span = (VENT_SLOTS - 1) * VENT_PITCH;
        for (i = [0 : VENT_SLOTS - 1])
            translate([cx,
                       cy - total_span/2 + i * VENT_PITCH,
                       -0.1])
                slot(VENT_SLOT_L, VENT_SLOT_W, LID_TOP_T + 0.2);

        // ── Label recess (optional — for a small label / engraving) ──────────
        // translate([cx - 8, cy + total_span/2 + 3, LID_TOP_T - 0.4])
        //     cube([16, 4, 0.5]);
    }
}


// ═══════════════════════════════════════════════════════
//  RENDER
//  Both parts shown side-by-side for preview.
//  When exporting STL: comment out the other part.
// ═══════════════════════════════════════════════════════

// BODY (print as-is, flat bottom down)
body();

// LID (shown flipped for printing — flat side will be print bed)
translate([0, BODY_W + 8, LID_TOP_T])
    rotate([180, 0, 0])
        lid();


// ═══════════════════════════════════════════════════════
//  COMPONENT PREVIEW (comment out before exporting STL)
//  Uncomment to visualise component placement inside body
// ═══════════════════════════════════════════════════════

//SHOW_COMPONENTS = true;   // set to false to hide
//if (SHOW_COMPONENTS) {
//    color("SteelBlue", 0.7)  // LiPo
//        translate([WALL + (IN_L - BAT_L)/2,
//                   WALL + (IN_W - BAT_W)/2,
//                   WALL])
//            cube([BAT_L, BAT_W, BAT_H]);
//
//    color("Green", 0.8)      // ESP32-C6 SuperMini
//        translate([WALL,
//                   WALL + (IN_W - ESP_W)/2,
//                   Z_SHELF1 + SHELF_H])
//            cube([ESP_L, ESP_W, ESP_H]);
//
//    color("OrangeRed", 0.8)  // SCD-41
//        translate([WALL + (IN_L - SCD_L)/2,
//                   WALL + (IN_W - SCD_W)/2,
//                   Z_SCD])
//            cube([SCD_L, SCD_W, SCD_H]);
//}
