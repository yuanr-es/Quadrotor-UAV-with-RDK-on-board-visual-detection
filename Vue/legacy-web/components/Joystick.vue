<template>
  <view class="joystick-row">
    <view class="joystick-label">{{ label }}</view>
    <view
      class="joystick-area"
      @touchstart="onStart"
      @touchmove="onMove"
      @touchend="onEnd"
    >
      <view
        class="joystick-knob"
        :style="{
          left: position.x + '%',
          top: position.y + '%'
        }"
      ></view>
      <view class="joystick-cross-h"></view>
      <view class="joystick-cross-v"></view>
    </view>
    <view class="joystick-values">
      <text>{{ channelA }}: {{ valueA }}</text>
      <text>{{ channelB }}: {{ valueB }}</text>
    </view>
  </view>
</template>

<script>
import uni from '@/uni-mock.js'

export default {
  name: 'Joystick',
  props: {
    label: { type: String, default: '摇杆' },
    channelA: { type: String, default: 'CH1' },
    channelB: { type: String, default: 'CH2' }
  },
  emits: ['change'],
  data() {
    return {
      position: { x: 50, y: 50 }
    }
  },
  computed: {
    valueA() {
      return Math.round(100 - this.position.y) + '%'
    },
    valueB() {
      const dx = this.position.x - 50
      const dy = 50 - this.position.y
      const angle = Math.atan2(dx, dy) * (180 / Math.PI)
      return Math.round(angle) + '°'
    }
  },
  methods: {
    onStart(e) { this.update(e) },
    onMove(e) { this.update(e) },
    onEnd() {
      this.position = { x: 50, y: 50 }
      this.emitChange()
    },
    update(e) {
      const touch = e.touches[0]
      if (!touch) return

      const query = uni.createSelectorQuery().in(this)
      query.select('.joystick-area').boundingClientRect(rect => {
        if (!rect) return
        let px = ((touch.clientX - rect.left) / rect.width) * 100
        let py = ((touch.clientY - rect.top) / rect.height) * 100
        px = Math.max(5, Math.min(95, px))
        py = Math.max(5, Math.min(95, py))

        this.position = { x: Math.round(px), y: Math.round(py) }
        this.emitChange()
      }).exec()
    },
    emitChange() {
      this.$emit('change', {
        x: this.position.x,
        y: this.position.y,
        valueA: this.valueA,
        valueB: this.valueB
      })
    }
  }
}
</script>

<style scoped>
.joystick-row {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-bottom: 16rpx;
  flex: 1;
}

.joystick-label {
  font-size: 18rpx;
  color: #888;
  margin-bottom: 6rpx;
}

.joystick-area {
  width: 160rpx;
  height: 160rpx;
  background: #0f0f1a;
  border: 3rpx solid #2a2a4a;
  border-radius: 50%;
  position: relative;
}

.joystick-cross-h {
  position: absolute;
  top: 50%; left: 10%; right: 10%;
  height: 1rpx;
  background: #2a2a4a;
  transform: translateY(-50%);
}

.joystick-cross-v {
  position: absolute;
  left: 50%; top: 10%; bottom: 10%;
  width: 1rpx;
  background: #2a2a4a;
  transform: translateX(-50%);
}

.joystick-knob {
  position: absolute;
  width: 56rpx;
  height: 56rpx;
  background: radial-gradient(circle, #00d4ff, #0088aa);
  border-radius: 50%;
  transform: translate(-50%, -50%);
  box-shadow: 0 0 16rpx rgba(0,212,255,0.5);
  z-index: 2;
}

.joystick-values {
  display: flex;
  gap: 16rpx;
  margin-top: 6rpx;
  font-size: 18rpx;
  color: #888;
}
</style>
