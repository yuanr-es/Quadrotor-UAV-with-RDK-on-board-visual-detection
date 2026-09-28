<template>
  <view class="left-panel">
    <view class="panel-title">飞行状态</view>

    <StatusCard label="GPS 坐标">
      <view class="status-value">{{ gps.lat }}</view>
      <view class="status-value-sub">{{ gps.lng }}</view>
    </StatusCard>

    <StatusCard label="高度">
      <view class="status-value">{{ altitude }} m</view>
    </StatusCard>

    <StatusCard label="水平速度">
      <view class="status-value">{{ horizontalSpeed }} m/s</view>
    </StatusCard>

    <StatusCard label="垂直速度">
      <view class="status-value">{{ verticalSpeed }} m/s</view>
    </StatusCard>

    <StatusCard label="距起飞点">
      <view class="status-value">{{ distance }} m</view>
    </StatusCard>

    <StatusCard label="飞行模式">
      <view class="status-value mode-gps">{{ flightMode }}</view>
    </StatusCard>

    <StatusCard label="IMU 状态">
      <view class="status-value" :class="imuOk ? 'status-ok' : 'status-err'">
        {{ imuOk ? '正常' : '异常' }}
      </view>
    </StatusCard>

    <StatusCard label="指南针">
      <view class="status-value" :class="compassOk ? 'status-ok' : 'status-err'">
        {{ compassOk ? '正常' : '异常' }}
      </view>
    </StatusCard>

    <view class="battery-section">
      <view class="panel-title">电池</view>
      <view class="battery-percent" :style="{ color: batteryColor }">{{ battery }}%</view>
      <view class="battery-bar-wrap">
        <view class="battery-bar" :style="{ width: battery + '%' }"></view>
      </view>
      <view class="battery-info">
        <text>电压: {{ voltage }}V</text>
        <text>剩余: {{ remainingTime }}min</text>
      </view>
    </view>
  </view>
</template>

<script>
import StatusCard from './StatusCard.vue'

export default {
  name: 'StatusPanel',
  components: { StatusCard },
  props: {
    gps: { type: Object, default: () => ({ lat: '--', lng: '--' }) },
    altitude: { type: [String, Number], default: '--' },
    horizontalSpeed: { type: [String, Number], default: '--' },
    verticalSpeed: { type: [String, Number], default: '--' },
    distance: { type: [String, Number], default: '--' },
    flightMode: { type: String, default: '--' },
    imuOk: { type: Boolean, default: true },
    compassOk: { type: Boolean, default: true },
    battery: { type: [String, Number], default: 0 },
    voltage: { type: [String, Number], default: '--' },
    remainingTime: { type: [String, Number], default: '--' }
  },
  computed: {
    batteryColor() {
      const b = Number(this.battery)
      if (b > 50) return '#23d160'
      if (b > 20) return '#ffdd57'
      return '#ff3860'
    }
  }
}
</script>

<style scoped>
.left-panel {
  width: 220rpx;
  background: #141428;
  padding: 16rpx 12rpx;
  overflow-y: auto;
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  gap: 8rpx;
}

.panel-title {
  font-size: 24rpx;
  color: #00d4ff;
  font-weight: bold;
  margin-bottom: 8rpx;
  padding-bottom: 8rpx;
  border-bottom: 2rpx solid #2a2a4a;
}

.status-value { font-size: 24rpx; color: #e0e0e0; font-weight: bold; }
.status-value-sub { font-size: 20rpx; color: #aaa; }
.mode-gps { color: #23d160 !important; }
.status-ok { color: #23d160 !important; }
.status-err { color: #ff3860 !important; }

.battery-section {
  margin-top: 12rpx;
  background: #1a1a2e;
  border-radius: 8rpx;
  padding: 12rpx 14rpx;
}

.battery-percent {
  font-size: 32rpx;
  font-weight: bold;
  text-align: center;
}

.battery-bar-wrap {
  height: 10rpx;
  background: #2a2a4a;
  border-radius: 5rpx;
  margin: 8rpx 0;
  overflow: hidden;
}

.battery-bar {
  height: 100%;
  background: linear-gradient(90deg, #23d160, #ffdd57, #ff3860);
  border-radius: 5rpx;
  transition: width 0.5s;
}

.battery-info {
  display: flex;
  justify-content: space-between;
  font-size: 18rpx;
  color: #888;
}
</style>
