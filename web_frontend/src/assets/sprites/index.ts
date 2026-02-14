/**
 * Inline SVG sprite constants for warehouse visualization.
 * All sprites are encoded as data URLs for zero external dependencies.
 */

/**
 * Robot sprite - top-down AGV/forklift view (~40x40px)
 * Features: dark gray body with rounded corners, forks at front, direction indicator
 */
export const ROBOT_SPRITE = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40" viewBox="0 0 40 40">
  <!-- Robot body -->
  <rect x="8" y="10" width="24" height="22" rx="3" ry="3" fill="#4a5568" stroke="#2d3748" stroke-width="1.5"/>

  <!-- Wheels -->
  <rect x="5" y="12" width="4" height="8" rx="1" fill="#1a202c"/>
  <rect x="31" y="12" width="4" height="8" rx="1" fill="#1a202c"/>
  <rect x="5" y="22" width="4" height="8" rx="1" fill="#1a202c"/>
  <rect x="31" y="22" width="4" height="8" rx="1" fill="#1a202c"/>

  <!-- Forks (at front/top) -->
  <rect x="12" y="2" width="3" height="10" rx="1" fill="#718096"/>
  <rect x="25" y="2" width="3" height="10" rx="1" fill="#718096"/>

  <!-- Direction indicator (triangle pointing up/forward) -->
  <polygon points="20,6 16,12 24,12" fill="#48bb78"/>

  <!-- Center detail -->
  <circle cx="20" cy="21" r="4" fill="#2d3748"/>
  <circle cx="20" cy="21" r="2" fill="#68d391"/>
</svg>
`)}`;

/**
 * Red crate sprite - top-down box view (~30x30px)
 */
export const CRATE_RED = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="30" height="30" viewBox="0 0 30 30">
  <!-- Box body -->
  <rect x="2" y="2" width="26" height="26" rx="2" fill="#ef4444" stroke="#b91c1c" stroke-width="2"/>

  <!-- X tape pattern -->
  <line x1="6" y1="6" x2="24" y2="24" stroke="#fca5a5" stroke-width="2" stroke-linecap="round"/>
  <line x1="24" y1="6" x2="6" y2="24" stroke="#fca5a5" stroke-width="2" stroke-linecap="round"/>

  <!-- Edge highlights -->
  <line x1="4" y1="4" x2="26" y2="4" stroke="#f87171" stroke-width="1"/>
  <line x1="4" y1="4" x2="4" y2="26" stroke="#f87171" stroke-width="1"/>
</svg>
`)}`;

/**
 * Green crate sprite - top-down box view (~30x30px)
 */
export const CRATE_GREEN = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="30" height="30" viewBox="0 0 30 30">
  <!-- Box body -->
  <rect x="2" y="2" width="26" height="26" rx="2" fill="#22c55e" stroke="#15803d" stroke-width="2"/>

  <!-- X tape pattern -->
  <line x1="6" y1="6" x2="24" y2="24" stroke="#86efac" stroke-width="2" stroke-linecap="round"/>
  <line x1="24" y1="6" x2="6" y2="24" stroke="#86efac" stroke-width="2" stroke-linecap="round"/>

  <!-- Edge highlights -->
  <line x1="4" y1="4" x2="26" y2="4" stroke="#4ade80" stroke-width="1"/>
  <line x1="4" y1="4" x2="4" y2="26" stroke="#4ade80" stroke-width="1"/>
</svg>
`)}`;

/**
 * Blue crate sprite - top-down box view (~30x30px)
 */
export const CRATE_BLUE = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="30" height="30" viewBox="0 0 30 30">
  <!-- Box body -->
  <rect x="2" y="2" width="26" height="26" rx="2" fill="#3b82f6" stroke="#1d4ed8" stroke-width="2"/>

  <!-- X tape pattern -->
  <line x1="6" y1="6" x2="24" y2="24" stroke="#93c5fd" stroke-width="2" stroke-linecap="round"/>
  <line x1="24" y1="6" x2="6" y2="24" stroke="#93c5fd" stroke-width="2" stroke-linecap="round"/>

  <!-- Edge highlights -->
  <line x1="4" y1="4" x2="26" y2="4" stroke="#60a5fa" stroke-width="1"/>
  <line x1="4" y1="4" x2="4" y2="26" stroke="#60a5fa" stroke-width="1"/>
</svg>
`)}`;

/**
 * Yellow crate sprite - top-down box view (~30x30px)
 */
export const CRATE_YELLOW = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="30" height="30" viewBox="0 0 30 30">
  <!-- Box body -->
  <rect x="2" y="2" width="26" height="26" rx="2" fill="#eab308" stroke="#a16207" stroke-width="2"/>

  <!-- X tape pattern -->
  <line x1="6" y1="6" x2="24" y2="24" stroke="#fef08a" stroke-width="2" stroke-linecap="round"/>
  <line x1="24" y1="6" x2="6" y2="24" stroke="#fef08a" stroke-width="2" stroke-linecap="round"/>

  <!-- Edge highlights -->
  <line x1="4" y1="4" x2="26" y2="4" stroke="#facc15" stroke-width="1"/>
  <line x1="4" y1="4" x2="4" y2="26" stroke="#facc15" stroke-width="1"/>
</svg>
`)}`;

/**
 * Floor tile sprite - warehouse concrete pattern (~60x60px, tileable)
 */
export const FLOOR_TILE = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="60" height="60" viewBox="0 0 60 60">
  <!-- Base concrete color -->
  <rect width="60" height="60" fill="#374151"/>

  <!-- Subtle texture noise -->
  <rect x="0" y="0" width="60" height="60" fill="#3f4a5a" opacity="0.3"/>

  <!-- Grid lines -->
  <line x1="0" y1="30" x2="60" y2="30" stroke="#2d3748" stroke-width="1"/>
  <line x1="30" y1="0" x2="30" y2="60" stroke="#2d3748" stroke-width="1"/>

  <!-- Subtle edge markers -->
  <line x1="0" y1="0" x2="60" y2="0" stroke="#4a5568" stroke-width="0.5"/>
  <line x1="0" y1="0" x2="0" y2="60" stroke="#4a5568" stroke-width="0.5"/>

  <!-- Corner dots for alignment reference -->
  <circle cx="5" cy="5" r="1" fill="#4a5568" opacity="0.5"/>
  <circle cx="55" cy="5" r="1" fill="#4a5568" opacity="0.5"/>
  <circle cx="5" cy="55" r="1" fill="#4a5568" opacity="0.5"/>
  <circle cx="55" cy="55" r="1" fill="#4a5568" opacity="0.5"/>
</svg>
`)}`;

/**
 * Zone marker sprite - striped hazard pattern (~100x100px, semi-transparent)
 */
export const ZONE_MARKER = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100">
  <defs>
    <!-- Diagonal stripe pattern -->
    <pattern id="hazardStripes" patternUnits="userSpaceOnUse" width="14" height="14" patternTransform="rotate(45)">
      <rect width="7" height="14" fill="#fbbf24" opacity="0.4"/>
      <rect x="7" width="7" height="14" fill="#1f2937" opacity="0.3"/>
    </pattern>
  </defs>

  <!-- Background circle with stripes -->
  <circle cx="50" cy="50" r="45" fill="url(#hazardStripes)"/>

  <!-- Border -->
  <circle cx="50" cy="50" r="45" fill="none" stroke="#fbbf24" stroke-width="3" opacity="0.6"/>

  <!-- Inner circle for visual clarity -->
  <circle cx="50" cy="50" r="35" fill="none" stroke="#fbbf24" stroke-width="1" opacity="0.3"/>
</svg>
`)}`;

/**
 * Wall texture sprite - industrial metal/concrete (~20x60px, tileable horizontally)
 */
export const WALL_TEXTURE = `data:image/svg+xml,${encodeURIComponent(`
<svg xmlns="http://www.w3.org/2000/svg" width="20" height="60" viewBox="0 0 20 60">
  <!-- Base wall color -->
  <rect width="20" height="60" fill="#6b7280"/>

  <!-- Metal panel sections -->
  <rect x="1" y="1" width="18" height="18" fill="#9ca3af" stroke="#4b5563" stroke-width="1"/>
  <rect x="1" y="21" width="18" height="18" fill="#9ca3af" stroke="#4b5563" stroke-width="1"/>
  <rect x="1" y="41" width="18" height="18" fill="#9ca3af" stroke="#4b5563" stroke-width="1"/>

  <!-- Panel rivets/bolts -->
  <circle cx="4" cy="4" r="1.5" fill="#4b5563"/>
  <circle cx="16" cy="4" r="1.5" fill="#4b5563"/>
  <circle cx="4" cy="16" r="1.5" fill="#4b5563"/>
  <circle cx="16" cy="16" r="1.5" fill="#4b5563"/>

  <circle cx="4" cy="24" r="1.5" fill="#4b5563"/>
  <circle cx="16" cy="24" r="1.5" fill="#4b5563"/>
  <circle cx="4" cy="36" r="1.5" fill="#4b5563"/>
  <circle cx="16" cy="36" r="1.5" fill="#4b5563"/>

  <circle cx="4" cy="44" r="1.5" fill="#4b5563"/>
  <circle cx="16" cy="44" r="1.5" fill="#4b5563"/>
  <circle cx="4" cy="56" r="1.5" fill="#4b5563"/>
  <circle cx="16" cy="56" r="1.5" fill="#4b5563"/>

  <!-- Highlight on top edge -->
  <line x1="0" y1="0" x2="20" y2="0" stroke="#d1d5db" stroke-width="1"/>
</svg>
`)}`;

/**
 * Map of crate colors to their sprite data URLs
 */
export const CRATE_SPRITES: Record<string, string> = {
  red: CRATE_RED,
  green: CRATE_GREEN,
  blue: CRATE_BLUE,
  yellow: CRATE_YELLOW,
};
