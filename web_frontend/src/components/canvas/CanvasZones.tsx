import { Image, Circle } from 'react-konva';
import { Entity } from '../../store/appStore';
import { useSprite } from '../../hooks/useSprite';
import { ZONE_MARKER } from '../../assets/sprites';
import { CoordinateTransform } from '../../utils/transforms';

/** Default zone radius in meters */
const DEFAULT_ZONE_RADIUS = 0.5;

interface CanvasZonesProps {
  zones: Entity[];
  scale: number;
  canvasSize: number;
}

/**
 * Renders zone markers on the canvas.
 * Zones are displayed as circular markers using either a sprite image
 * or a fallback circle shape if the sprite fails to load.
 */
export function CanvasZones({ zones, scale, canvasSize }: CanvasZonesProps): React.ReactElement {
  const zoneImage = useSprite(ZONE_MARKER);
  const transform = new CoordinateTransform(canvasSize / scale, canvasSize);

  const zoneRadius = DEFAULT_ZONE_RADIUS * scale;
  const zoneDisplaySize = zoneRadius * 2;

  return (
    <>
      {zones.map((zone) => {
        const [cx, cy] = transform.worldToCanvas(zone.x, zone.y);
        return zoneImage ? (
          <Image
            key={zone.id}
            image={zoneImage}
            x={cx}
            y={cy}
            width={zoneDisplaySize}
            height={zoneDisplaySize}
            offsetX={zoneDisplaySize / 2}
            offsetY={zoneDisplaySize / 2}
          />
        ) : (
          <Circle
            key={zone.id}
            x={cx}
            y={cy}
            radius={zoneRadius}
            fill="rgba(100, 200, 100, 0.3)"
            stroke="#4ade80"
            strokeWidth={2}
          />
        );
      })}
    </>
  );
}
