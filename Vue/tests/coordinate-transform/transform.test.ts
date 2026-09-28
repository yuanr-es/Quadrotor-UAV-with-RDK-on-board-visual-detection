import { describe, it, expect } from 'vitest'
import {
  computeDisplayRect,
  containerPointToFrame,
  containerRectToFrameNormalized,
  toProtocolRect,
  frameNormalizedToPixel,
  rotatedFrameSize,
  ViewTransform
} from '../../domain/video/coordinate-transform'

function tf(overrides: Partial<ViewTransform> = {}): ViewTransform {
  return {
    rawFrame: { width: 1920, height: 1080 },
    rotation: 0,
    mirror: false,
    container: { width: 800, height: 600 },
    fit: 'contain',
    ...overrides
  }
}

describe('坐标系尺寸与显示矩形', () => {
  it('90/270 旋转交换宽高', () => {
    expect(rotatedFrameSize({ width: 1920, height: 1080 }, 90)).toEqual({
      width: 1080,
      height: 1920
    })
    expect(rotatedFrameSize({ width: 1920, height: 1080 }, 180)).toEqual({
      width: 1920,
      height: 1080
    })
  })

  it('contain 模式产生上下黑边', () => {
    const d = computeDisplayRect(tf())
    expect(d.width).toBeCloseTo(800)
    expect(d.height).toBeCloseTo(450)
    expect(d.x).toBeCloseTo(0)
    expect(d.y).toBeCloseTo(75)
  })

  it('cover 模式溢出容器（负偏移）', () => {
    const d = computeDisplayRect(tf({ fit: 'cover' }))
    expect(d.x).toBeLessThan(0)
    expect(d.height).toBeCloseTo(600)
  })
})

describe('点映射', () => {
  it('中心点始终映射到帧中心', () => {
    expect(containerPointToFrame(tf(), { x: 400, y: 300 })).toEqual({
      x: 960,
      y: 540
    })
  })

  it('黑边区域返回 null', () => {
    expect(containerPointToFrame(tf(), { x: 400, y: 10 })).toBeNull()
  })

  const square: Partial<ViewTransform> = {
    rawFrame: { width: 100, height: 100 },
    container: { width: 100, height: 100 }
  }

  it.each([
    [0, { x: 0, y: 0 }, { x: 0, y: 0 }],
    [90, { x: 0, y: 0 }, { x: 0, y: 100 }],
    [180, { x: 0, y: 0 }, { x: 100, y: 100 }],
    [270, { x: 0, y: 0 }, { x: 100, y: 0 }]
  ] as const)('旋转 %i°：显示左上角映射到预期帧坐标', (rotation, point, expected) => {
    const got = containerPointToFrame(tf({ ...square, rotation }), point)
    expect(got).toEqual(expected)
  })

  it('镜像：显示左侧映射到原始帧右侧', () => {
    const got = containerPointToFrame(tf({ ...square, mirror: true }), { x: 0, y: 0 })
    expect(got).toEqual({ x: 100, y: 0 })
  })

  it('镜像 + 旋转 90 组合', () => {
    // 显示左上 -> 先镜像 dx=1 -> rot90 -> {x:0,y:0}
    const got = containerPointToFrame(
      tf({ ...square, rotation: 90, mirror: true }),
      { x: 0, y: 0 }
    )
    expect(got).toEqual({ x: 0, y: 0 })
  })
})

describe('矩形映射', () => {
  it('覆盖整个显示区域 -> 归一化全幅', () => {
    const r = containerRectToFrameNormalized(tf(), { x: 0, y: 75, width: 800, height: 450 })
    expect(r).not.toBeNull()
    expect(r!.x).toBeCloseTo(0)
    expect(r!.y).toBeCloseTo(0)
    expect(r!.width).toBeCloseTo(1)
    expect(r!.height).toBeCloseTo(1)
  })

  it('完全落在黑边 -> null', () => {
    const r = containerRectToFrameNormalized(tf(), { x: 0, y: 0, width: 800, height: 10 })
    expect(r).toBeNull()
  })

  it('部分越界被裁剪', () => {
    const r = containerRectToFrameNormalized(tf(), {
      x: -100,
      y: 75,
      width: 500,
      height: 450
    })
    expect(r).not.toBeNull()
    expect(r!.x).toBeCloseTo(0)
    expect(r!.width).toBeCloseTo(0.5)
  })

  it('归一化矩形转像素', () => {
    const px = toProtocolRect(
      { x: 0.5, y: 0.5, width: 0.25, height: 0.25 },
      'pixel',
      { width: 1920, height: 1080 }
    )
    expect(px).toEqual({ x: 960, y: 540, width: 480, height: 270 })
  })

  it('frameNormalizedToPixel', () => {
    expect(frameNormalizedToPixel({ width: 640, height: 480 }, { x: 0.5, y: 0.25 })).toEqual({
      x: 320,
      y: 120
    })
  })
})
