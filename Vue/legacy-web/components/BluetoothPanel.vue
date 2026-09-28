<template>
  <view class="bluetooth-section">
    <view class="panel-title">蓝牙连接</view>

    <view class="bt-status">
      <view class="bt-indicator" :class="bt.connected ? 'bt-on' : 'bt-off'"></view>
      <text class="bt-status-text">{{ bt.connected ? '已连接' : '未连接' }}</text>
    </view>

    <view v-if="bt.connected && bt.device" class="bt-device">
      <text class="bt-device-name">🕹 {{ bt.device.name }}</text>
      <text class="bt-device-mac">{{ bt.device.mac }}</text>
    </view>

    <view class="bt-scan-btn" @tap="handleClick">
      <text>{{ bt.connected ? '断开连接' : (bt.scanning ? '扫描中...' : '扫描设备') }}</text>
    </view>

    <view v-if="bt.error" class="bt-error">{{ bt.error }}</view>

    <view v-if="bt.devices.length > 0 && !bt.connected" class="bt-device-list">
      <view
        v-for="d in bt.devices"
        :key="d.id"
        class="bt-device-item"
        @tap="onConnect(d)"
      >
        <text class="bt-device-item-name">{{ d.name }}</text>
        <text class="bt-device-item-rssi">{{ d.rssi }} dBm</text>
      </view>
    </view>
  </view>
</template>

<script>
import {
  useBluetooth,
  initBluetooth,
  startScan,
  connectDevice as btConnect,
  disconnectDevice as btDisconnect
} from '@/services/bluetooth.js'

export default {
  name: 'BluetoothPanel',
  data() {
    return {
      bt: useBluetooth()
    }
  },
  async mounted() {
    try { await initBluetooth() } catch (e) { /* 初始化失败在 bt.error 中显示 */ }
  },
  methods: {
    async handleClick() {
      if (this.bt.connected) {
        await btDisconnect()
      } else {
        await startScan()
      }
    },
    async onConnect(device) {
      await btConnect(device)
    }
  }
}
</script>

<style scoped>
.bluetooth-section {
  background: #1a1a2e;
  border-radius: 8rpx;
  padding: 12rpx 14rpx;
}

.panel-title {
  font-size: 24rpx;
  color: #00d4ff;
  font-weight: bold;
  margin-bottom: 8rpx;
  padding-bottom: 8rpx;
  border-bottom: 2rpx solid #2a2a4a;
}

.bt-status {
  display: flex;
  align-items: center;
  gap: 10rpx;
  margin-bottom: 10rpx;
}

.bt-indicator {
  width: 16rpx; height: 16rpx;
  border-radius: 50%;
}

.bt-on { background: #23d160; box-shadow: 0 0 10rpx #23d160; }
.bt-off { background: #666; }

.bt-status-text { font-size: 22rpx; color: #e0e0e0; }

.bt-device { margin-bottom: 10rpx; }

.bt-device-name {
  display: block;
  font-size: 20rpx; color: #e0e0e0;
}

.bt-device-mac { font-size: 16rpx; color: #666; }

.bt-scan-btn {
  background: #00d4ff;
  color: #0f0f1a;
  text-align: center;
  padding: 12rpx;
  border-radius: 8rpx;
  font-size: 22rpx;
  font-weight: bold;
}

.bt-error {
  margin-top: 8rpx;
  font-size: 18rpx;
  color: #ff3860;
}

.bt-device-list { margin-top: 12rpx; }

.bt-device-item {
  display: flex;
  justify-content: space-between;
  padding: 10rpx 0;
  border-bottom: 1rpx solid #2a2a4a;
}

.bt-device-item-name { font-size: 20rpx; color: #ccc; }
.bt-device-item-rssi { font-size: 18rpx; color: #888; }
</style>
