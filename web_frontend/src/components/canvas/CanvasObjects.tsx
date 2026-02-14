import { useMemo } from 'react';
import { Image, Circle } from 'react-konva';
import Konva from 'konva';
import { Entity } from '../../store/appStore';
import { useSprites } from '../../hooks/useSprite';
import { CRATE_SPRITES } from '../../assets/sprites/index';
import { CoordinateTransform } from '../../utils/transforms';

/** Object size in meters */
const OBJECT_SIZE = 0.5;

/** Fallback color map for objects when sprites are not available */
const COLOR_MAP: Record<string, string> = {
  red: '#ef4444',
  green: '#22c55e',
  blue: '#3b82f6',
  yellow: '#eab308',
};

interface CanvasObjectsProps {
  objects: Entity[];
  scale: number;
  canvasSize: number;
  onObjectMoved?: (id: string, worldX: number, worldY: number) => void;
}

/**
 * Renders draggable object sprites on the canvas.
 * Objects are displayed using crate sprites or fallback circles.
 * Supports drag-and-drop interaction to reposition objects.
 */
export function CanvasObjects({
  objects,
  scale,
  canvasSize,
  onObjectMoved,
}: CanvasObjectsProps): React.ReactElement {
  const crateImages = useSprites(CRATE_SPRITES);

  // Memoize transform to avoid useMemo dependency issues
  const transform = useMemo(
    () => new CoordinateTransform(canvasSize / scale, canvasSize),
    [canvasSize, scale]
  );

  const objSize = OBJECT_SIZE * scale;

  const handleDragEnd = (objId: string) => (e: Konva.KonvaEventObject<DragEvent>) => {
    if (onObjectMoved) {
      const [wx, wy] = transform.canvasToWorld(e.target.x(), e.target.y());
      onObjectMoved(objId, wx, wy);
    }
  };

  return (
    <>
      {objects.map((obj) => {
        const [cx, cy] = transform.worldToCanvas(obj.x, obj.y);
        const crateImage = crateImages[obj.color || 'red'];

        return crateImage ? (
          <Image
            key={obj.id}
            image={crateImage}
            x={cx}
            y={cy}
            width={objSize}
            height={objSize}
            offsetX={objSize / 2}
            offsetY={objSize / 2}
            draggable
            onDragEnd={handleDragEnd(obj.id)}
          />
        ) : (
          <Circle
            key={obj.id}
            x={cx}
            y={cy}
            radius={(OBJECT_SIZE / 2) * scale}
            fill={COLOR_MAP[obj.color || 'red'] || '#888'}
            stroke="#fff"
            strokeWidth={2}
            draggable
            onDragEnd={handleDragEnd(obj.id)}
          />
        );
      })}
    </>
  );
}
