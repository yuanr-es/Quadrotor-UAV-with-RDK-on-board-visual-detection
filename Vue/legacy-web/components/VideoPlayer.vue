<template>
  <view class="center-panel">
    <view
      class="video-container"
      @touchstart="onTouchStart"
      @touchmove="onTouchMove"
      @touchend="onTouchEnd"
    >
      <!-- 真实视频 -->
      <video
        v-if="videoSrc"
        :src="videoSrc"
        class="video-element"
        object-fit="contain"
        autoplay
        muted
      ></video>

      <!-- 无信号占位 -->
      <view v-else class="video-placeholder">
        <view class="video-icon">📷</view>
        <view class="video-text">视频画面</view>
        <view class="video-sub-text">等待视频信号接入...</view>
      </view>

      <!-- 触屏绘制的方框叠加层 -->
      <view
        v-if="drawing"
        class="track-rect"
        :style="rectStyle"
      ></view>

      <!-- OSD 叠加 -->
      <view class="video-osd">
        <view class="osd-top-left">
          <text v-if="isRecording" class="rec-text">REC ●</text>
          <text class="res-text">{{ resolution }}</text>
        </view>
        <view v-if="rectCoords" class="osd-top-right">
          <text class="track-info">
            X:{{ rectCoords.x }} Y:{{ rectCoords.y }} W:{{ rectCoords.hx }} H:{{ rectCoords.hy }}
          </text>
        </view>
      </view>

      <!-- 操作提示 -->
      <view v-if="!rectCoords && !drawing" class="draw-hint">滑动画面框选目标</view>
    </view>
  </view>
</template>

<script>
export default {
  name: 'VideoPlayer',
  props: {
    videoSrc: { type: String, default: '' },
    isRecording: { type: Boolean, default: false },
    resolution: { type: String, default: '1080P 60FPS' }
  },
  emits: ['track-rect'],
  data() {
    return {
      drawing: false,
      startX: 0,
      startY: 0,
      rect: { left: 0, top: 0, width: 0, height: 0 },
      rectCoords: null
    }
  },
  computed: {
    rectStyle() {
      const r = this.rect
      return {
        left: r.left + 'px',
        top: r.top + 'px',
        width: r.width + 'px',
        height: r.height + 'px'
      }
    }
  },
  methods: {
    getContainerRect() {
      const el = this.$el.querySelector('.video-container')
      if (!el) return null
      return el.getBoundingClientRect()
    },
    onTouchStart(e) {
      const touch = e.touches[0]
      const container = this.getContainerRect()
      if (!container) return

      this.drawing = true
      this.startX = touch.clientX - container.left
      this.startY = touch.clientY - container.top
      this.rect = { left: this.startX, top: this.startY, width: 0, height: 0 }
    },
    onTouchMove(e) {
      if (!this.drawing) return
      const touch = e.touches[0]
      const container = this.getContainerRect()
      if (!container) return

      const cx = touch.clientX - container.left
      const cy = touch.clientY - container.top

      const left = Math.min(this.startX, cx)
      const top = Math.min(this.startY, cy)
      const right = Math.max(this.startX, cx)
      const bottom = Math.max(this.startY, cy)

      this.rect = {
        left: Math.max(0, left),
        top: Math.max(0, top),
        width: Math.min(right, container.width) - Math.max(0, left),
        height: Math.min(bottom, container.height) - Math.max(0, top)
      }
    },
    onTouchEnd() {
      if (!this.drawing) return
      this.drawing = false

      // 最小方框阈值：小于 20px 视为误触，忽略
      if (this.rect.width < 20 || this.rect.height < 20) {
        this.rect = { left: 0, top: 0, width: 0, height: 0 }
        return
      }

      const r = this.rect
      this.rectCoords = {
        x: Math.round(r.left),
        y: Math.round(r.top),
        hx: Math.round(r.width),
        hy: Math.round(r.height)
      }

      this.$emit('track-rect', this.rectCoords)
    }
  }
}
</script>

<style scoped>
.center-panel {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 8rpx;
  background: #0a0a14;
}

.video-container {
  width: 100%;
  height: 100%;
  position: relative;
  border-radius: 8rpx;
  overflow: hidden;
  background: #000;
  touch-action: none;
}

.video-element {
  width: 100%;
  height: 100%;
}

.video-placeholder {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: #111122;
  border: 2rpx dashed #2a2a4a;
  border-radius: 8rpx;
}

.video-icon { font-size: 80rpx; margin-bottom: 16rpx; opacity: 0.5; }
.video-text { font-size: 32rpx; color: #666; margin-bottom: 8rpx; }
.video-sub-text { font-size: 22rpx; color: #444; }

/* 触屏框选 */
.track-rect {
  position: absolute;
  border: 3rpx solid #00d4ff;
  background: rgba(0, 212, 255, 0.15);
  box-shadow: 0 0 10rpx rgba(0, 212, 255, 0.4);
  pointer-events: none;
  z-index: 10;
}

.video-osd {
  position: absolute;
  top: 0; left: 0; right: 0; bottom: 0;
  pointer-events: none;
  z-index: 5;
}

.osd-top-left {
  position: absolute;
  top: 12rpx; left: 16rpx;
  display: flex; gap: 16rpx;
  font-size: 18rpx;
}

.rec-text { color: #ff3860; }
.res-text { color: #fff; }

.osd-top-right {
  position: absolute;
  top: 12rpx; right: 16rpx;
  font-size: 18rpx;
  color: #00d4ff;
  text-shadow: 0 0 4rpx rgba(0,0,0,0.8);
}

.draw-hint {
  position: absolute;
  bottom: 16rpx;
  left: 50%;
  transform: translateX(-50%);
  font-size: 20rpx;
  color: rgba(255,255,255,0.35);
  pointer-events: none;
  z-index: 5;
}
</style>
