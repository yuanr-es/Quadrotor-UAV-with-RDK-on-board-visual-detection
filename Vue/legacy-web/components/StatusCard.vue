<template>
  <view class="status-card">
    <view class="status-label">{{ label }}</view>
    <slot></slot>
  </view>
</template>

<script>
export default {
  name: 'StatusCard',
  props: {
    label: { type: String, required: true }
  }
}
</script>

<style scoped>
.status-card {
  background: #1a1a2e;
  border-radius: 8rpx;
  padding: 10rpx 14rpx;
}

.status-label {
  font-size: 18rpx;
  color: #888;
  margin-bottom: 2rpx;
}
</style>
