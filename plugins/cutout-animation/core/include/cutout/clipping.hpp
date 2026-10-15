// Clipping attachments (plugin 1.12.0): draw items cut to a clip polygon on the CPU, so hosts
// draw the result with nothing but plugin-mesh-v1 (clipped items are ordinary mesh items).
// Positions are snapped to 1/4096 px, then plain IEEE arithmetic (sqrt, no libm) and ordered
// containers only: the native core and Wasm cut identically.
#pragma once
#include <vector>
#include "runtime.hpp"

namespace cutout {
/**
 * Convex parts of a closed polygon (x, y pairs, either orientation): ear-clipping triangulation,
 * then triangles merged greedily into convex polygons across shared diagonals (Hertel–Mehlhorn).
 * Each part has positive signed area. A convex polygon is one part; concave ones are supported.
 * Collinear and repeated points are dropped; empty when nothing with area remains. A polygon
 * that a pose has folded into self-intersection still gives parts (the leftover is fanned).
 */
std::vector<std::vector<double>> convexParts(const std::vector<double>& polygon);

/** One vertex being clipped: position and texture coordinate. */
struct ClipVertex { double x = 0, y = 0, u = 0, v = 0; };
/**
 * Sutherland–Hodgman: the convex polygon `subject` (any orientation) cut by the convex
 * positive-area polygon `clip` (x, y pairs). uvs are interpolated linearly along the cut edges;
 * each intersection is computed from its segment's endpoints in one canonical order, so two
 * triangles sharing an edge get bit-identical points there (no seams). Points within 1e-9 px of
 * an edge count as inside. Fewer than 3 vertices = nothing left.
 */
std::vector<ClipVertex> clipConvex(const std::vector<ClipVertex>& subject, const std::vector<double>& clip);

/** A clip polygon prepared for cutting items: its convex parts and their bounds. */
struct ClipRegion {
  std::vector<std::vector<double>> parts;
  double minX = 0, minY = 0, maxX = 0, maxY = 0;
};
ClipRegion clipRegion(const std::vector<double>& polygon);

/**
 * `item` cut by `region`, appended to `out`:
 *  - every triangle inside one convex part: the item unchanged (a region stays a region);
 *  - nothing of it inside: no item;
 *  - otherwise mesh items (`clipped` = true; a region becomes its two-triangle quad first, uvs
 *    over its picture or atlas sub-image) with shared vertices deduplicated, split so each has at
 *    most MAX_MESH_VERTICES vertices and MAX_MESH_TRIANGLES triangles (what hosts accept).
 * matrix / width / height / region stay the item's, as for any mesh item.
 */
void clipItem(const DrawItem& item, const ClipRegion& region, std::vector<DrawItem>& out);
}  // namespace cutout
