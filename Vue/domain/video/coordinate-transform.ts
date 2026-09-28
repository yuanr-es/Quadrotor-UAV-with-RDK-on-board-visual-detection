/**
 * 目标框坐标映射（§6）
 *
 * 尺寸层级：
 *   1. 原始视频帧 rawFrame
 *   2. 旋转后帧 rotatedFrame（90/270 时宽高互换）
 *   3. 组件容器 container
 *   4. 实际画面显示矩形 displayRect（contain/cover 会留黑边或裁剪）
 *
 * 触摸坐标按 container -> displayRect -> rotatedFrame -> rawFrame 逆变换，
 * 再输出协议要求的像素或归一化坐标。
 */

export interface Size {
  width: number
  height: number
}

export interface Rect {
  x: number
  y: number
  width: number
  height: number
}

export interface Point {
  x: number
  y: number
}

export type Rotation = 0 | 90 | 180 | 270
export type FitMode = 'contain' | 'cover'

export interface ViewTransform {
  rawFrame: Size
  rotation: Rotation
  mirror: boolean
  container: Size
  fit: FitMode
}

/** 旋转后帧尺寸（90/270 交换宽高）。 */
export function rotatedFrameSize(rawFrame: Size, rotation: Rotation): Size {
  return rotation === 90 || rotation === 270
    ? { width: rawFrame.height, height: rawFrame.width }
    : { width: rawFrame.width, height: rawFrame.height }
}

/** 实际画面在容器内的显示矩形。 */
export function computeDisplayRect(t: ViewTransform): Rect {
  const r = rotatedFrameSize(t.rawFrame, t.rotation)
  const scale =
    t.fit === 'contain'
      ? Math.min(t.container.width / r.width, t.container.height / r.height)
      : Math.max(t.container.width / r.width, t.container.height / r.height)
  const width = r.width * scale
  const height = r.height * scale
  return {
    x: (t.container.width - width) / 2,
    y: (t.container.height - height) / 2,
    width,
    height
  }
}

/** 容器内归一化坐标（相对显示矩形）。黑边区域返回 null。 */
export function containerPointToDisplayNormalized(
  t: ViewTransform,
  p: Point
): Point | null {
  const d = computeDisplayRect(t)
  if (p.x < d.x || p.y < d.y || p.x > d.x + d.width || p.y > d.y + d.height) {
    return null
  }
  return {
    x: (p.x - d.x) / d.width,
    y: (p.y - d.y) / d.height
  }
}

/** 显示坐标 -> 旋转后帧归一化坐标。 */
function displayToRotated(t: ViewTransform, p: Point): Point {
  let dx = p.x
  let dy = p.y
  if (t.mirror) dx = 1 - dx
  switch (t.rotation) {
    case 90:
      return { x: dy, y: 1 - dx }
    case 180:
      return { x: 1 - dx, y: 1 - dy }
    case 270:
      return { x: 1 - dy, y: dx }
    default:
      return { x: dx, y: dy }
  }
}

/**
 * 容器坐标点 -> 原始帧像素坐标。黑边区域返回 null。
 * 返回坐标不取整，便于上层按需处理。
 */
export function containerPointToFrame(t: ViewTransform, p: Point): Point | null {
  const norm = containerPointToDisplayNormalized(t, p)
  if (!norm) return null
  const rp = displayToRotated(t, norm)
  return { x: rp.x * t.rawFrame.width, y: rp.y * t.rawFrame.height }
}

/** 原始帧归一化坐标 -> 像素坐标（协议要求像素时使用）。 */
export function frameNormalizedToPixel(
  frame: Size,
  n: Point
): Point {
  return { x: n.x * frame.width, y: n.y * frame.height }
}

export interface NormalizedRect extends Rect {}

/**
 * 容器坐标矩形 -> 原始帧归一化矩形 [0,1]。
 * 会裁剪到实际画面区域并做逆变换与越界限制；完全落在黑边时返回 null。
 */
export function containerRectToFrameNormalized(
  t: ViewTransform,
  rect: Rect
): NormalizedRect | null {
  const corners = [
    { x: rect.x, y: rect.y },
    { x: rect.x + rect.width, y: rect.y },
    { x: rect.x, y: rect.y + rect.height },
    { x: rect.x + rect.width, y: rect.y + rect.height }
  ]

  const d = computeDisplayRect(t)
  // 裁剪到显示矩形
  const clamped = corners.map((c) => ({
    x: Math.min(Math.max(c.x, d.x), d.x + d.width),
    y: Math.min(Math.max(c.y, d.y), d.y + d.height)
  }))

  // 完全在显示矩形外的角判定：原矩形与显示矩形无交集
  const noIntersection =
    rect.x + rect.width <= d.x ||
    rect.x >= d.x + d.width ||
    rect.y + rect.height <= d.y ||
    rect.y >= d.y + d.height
  if (noIntersection) return null

  const mapped = clamped.map((c) => {
    const norm = {
      x: (c.x - d.x) / d.width,
      y: (c.y - d.y) / d.height
    }
    const rp = displayToRotated(t, norm)
    return {
      x: clamp01(rp.x),
      y: clamp01(rp.y)
    }
  })

  const xs = mapped.map((m) => m.x)
  const ys = mapped.map((m) => m.y)
  const minX = Math.min(...xs)
  const maxX = Math.max(...xs)
  const minY = Math.min(...ys)
  const maxY = Math.max(...ys)

  return {
    x: minX,
    y: minY,
    width: maxX - minX,
    height: maxY - minY
  }
}

/**
 * 归一化矩形 -> 协议坐标。字段统一为 x/y/width/height（修正原 hx/hy 歧义）。
 */
export function toProtocolRect(
  norm: NormalizedRect,
  mode: 'normalized' | 'pixel',
  frame: Size
): NormalizedRect {
  if (mode === 'pixel') {
    return {
      x: norm.x * frame.width,
      y: norm.y * frame.height,
      width: norm.width * frame.width,
      height: norm.height * frame.height
    }
  }
  return { ...norm }
}

function clamp01(v: number): number {
  return Math.min(1, Math.max(0, v))
}
