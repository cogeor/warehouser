import { useEffect, useState } from 'react';

/**
 * Hook that converts an SVG data URL to an HTMLImageElement for use with Konva Image component.
 *
 * @param dataUrl - SVG data URL string (e.g., from sprite constants)
 * @returns HTMLImageElement when loaded, null while loading
 *
 * @example
 * ```tsx
 * import { Image } from 'react-konva';
 * import { ROBOT_SPRITE } from '../assets/sprites';
 * import { useSprite } from '../hooks/useSprite';
 *
 * function RobotComponent({ x, y }: { x: number; y: number }) {
 *   const robotImage = useSprite(ROBOT_SPRITE);
 *
 *   if (!robotImage) return null;
 *
 *   return <Image image={robotImage} x={x} y={y} />;
 * }
 * ```
 */
export function useSprite(dataUrl: string): HTMLImageElement | null {
  const [image, setImage] = useState<HTMLImageElement | null>(null);

  useEffect(() => {
    const img = new Image();
    img.onload = () => setImage(img);
    img.onerror = () => {
      console.error('Failed to load sprite:', dataUrl.slice(0, 50) + '...');
      setImage(null);
    };
    img.src = dataUrl;

    return () => {
      img.onload = null;
      img.onerror = null;
    };
  }, [dataUrl]);

  return image;
}

/**
 * Hook that preloads multiple sprites and returns them as a map.
 *
 * @param sprites - Record of sprite names to data URLs
 * @returns Record of sprite names to loaded HTMLImageElements (or null if not loaded)
 *
 * @example
 * ```tsx
 * import { CRATE_SPRITES } from '../assets/sprites';
 * import { useSprites } from '../hooks/useSprite';
 *
 * function CrateComponent({ color, x, y }: { color: string; x: number; y: number }) {
 *   const crateImages = useSprites(CRATE_SPRITES);
 *   const image = crateImages[color];
 *
 *   if (!image) return null;
 *
 *   return <Image image={image} x={x} y={y} />;
 * }
 * ```
 */
export function useSprites(
  sprites: Record<string, string>
): Record<string, HTMLImageElement | null> {
  const [images, setImages] = useState<Record<string, HTMLImageElement | null>>({});

  useEffect(() => {
    const loadedImages: Record<string, HTMLImageElement | null> = {};
    let mounted = true;

    const entries = Object.entries(sprites);
    let loadedCount = 0;

    entries.forEach(([name, dataUrl]) => {
      const img = new Image();
      img.onload = () => {
        loadedImages[name] = img;
        loadedCount++;
        if (mounted && loadedCount === entries.length) {
          setImages({ ...loadedImages });
        }
      };
      img.onerror = () => {
        loadedImages[name] = null;
        loadedCount++;
        if (mounted && loadedCount === entries.length) {
          setImages({ ...loadedImages });
        }
      };
      img.src = dataUrl;
    });

    return () => {
      mounted = false;
    };
  }, [sprites]);

  return images;
}
