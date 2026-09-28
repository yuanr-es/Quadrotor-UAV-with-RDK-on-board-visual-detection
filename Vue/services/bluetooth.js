/**
 * 蓝牙连接服务
 *
 * 封装 uni-app 蓝牙 API，对外提供统一的连接管理和数据发送接口。
 * 真机使用 uni.xxx API；浏览器调试时自动降级为模拟模式。
 */

import { reactive, readonly } from 'vue'

const IS_BROWSER = typeof window !== 'undefined' && !window.uni?.openBluetoothAdapter

const SERVICE_UUID = '0000FFE0-0000-1000-8000-00805F9B34FB'
const CHAR_UUID = '0000FFE1-0000-1000-8000-00805F9B34FB'

const state = reactive({
  initialized: false,
  scanning: false,
  connected: false,
  device: null,
  devices: [],
  error: ''
})

let deviceId = null

// ==================== 真机 uni-app API ====================

async function nativeInit() {
  return new Promise((resolve, reject) => {
    uni.openBluetoothAdapter({
      success: () => { state.initialized = true; resolve() },
      fail: (err) => {
        state.error = '蓝牙初始化失败: ' + (err.errMsg || err.message)
        reject(new Error(state.error))
      }
    })
  })
}

async function nativeScan() {
  return new Promise((resolve, reject) => {
    state.scanning = true
    state.devices = []
    uni.startBluetoothDevicesDiscovery({
      success: () => {
        uni.onBluetoothDeviceFound((res) => {
          res.devices.forEach(d => {
            if (d.name && !state.devices.find(e => e.id === d.deviceId)) {
              state.devices.push({
                id: d.deviceId,
                name: d.name || d.localName || '未知设备',
                mac: d.deviceId,
                rssi: d.RSSI
              })
            }
          })
        })
        resolve()
      },
      fail: (err) => {
        state.scanning = false
        state.error = '扫描失败: ' + (err.errMsg || err.message)
        reject(new Error(state.error))
      }
    })
  })
}

async function nativeConnect(device) {
  return new Promise((resolve, reject) => {
    uni.stopBluetoothDevicesDiscovery()
    state.scanning = false

    uni.createBLEConnection({
      deviceId: device.id,
      success: () => {
        deviceId = device.id
        state.connected = true
        state.device = device
        resolve()
      },
      fail: (err) => {
        state.error = '连接失败: ' + (err.errMsg || err.message)
        reject(new Error(state.error))
      }
    })
  })
}

async function nativeDisconnect() {
  return new Promise((resolve) => {
    if (deviceId) {
      uni.closeBLEConnection({ deviceId })
    }
    uni.closeBluetoothAdapter({
      success: () => {
        state.initialized = false
        state.connected = false
        state.device = null
        deviceId = null
        resolve()
      }
    })
  })
}

async function nativeSend(data) {
  return new Promise((resolve, reject) => {
    if (!deviceId) {
      reject(new Error('未连接蓝牙设备'))
      return
    }
    // 将数据转 ArrayBuffer
    const str = JSON.stringify(data)
    const buffer = new ArrayBuffer(str.length)
    const view = new Uint8Array(buffer)
    for (let i = 0; i < str.length; i++) {
      view[i] = str.charCodeAt(i)
    }
    uni.writeBLECharacteristicValue({
      deviceId,
      serviceId: SERVICE_UUID,
      characteristicId: CHAR_UUID,
      value: buffer,
      success: () => resolve(),
      fail: (err) => reject(new Error('发送失败: ' + (err.errMsg || err.message)))
    })
  })
}

// ==================== 浏览器模拟模式 ====================

async function mockInit() {
  state.initialized = true
}

async function mockScan() {
  state.scanning = true
  state.devices = []
  await delay(1200)
  state.devices = [
    { id: 'mock-001', name: 'Bear2Pro RC', mac: 'AA:BB:CC:11:22:33', rssi: -38 },
    { id: 'mock-002', name: 'BT-Remote-01', mac: 'DD:EE:FF:44:55:66', rssi: -72 }
  ]
  state.scanning = false
}

async function mockConnect(device) {
  state.scanning = false
  await delay(500)
  state.connected = true
  state.device = device
}

async function mockDisconnect() {
  state.connected = false
  state.device = null
}

async function mockSend(data) {
  console.log('[BT Mock] 发送数据:', JSON.stringify(data))
}

function delay(ms) {
  return new Promise(r => setTimeout(r, ms))
}

// ==================== 统一导出接口 ====================

const api = IS_BROWSER
  ? { init: mockInit, scan: mockScan, connect: mockConnect, disconnect: mockDisconnect, send: mockSend }
  : { init: nativeInit, scan: nativeScan, connect: nativeConnect, disconnect: nativeDisconnect, send: nativeSend }

export async function initBluetooth() { return api.init() }
export async function startScan() { return api.scan() }
export async function connectDevice(device) { return api.connect(device) }
export async function disconnectDevice() { return api.disconnect() }

/**
 * 发送数据到已连接的蓝牙设备
 * @param {object} data - 将被 JSON 序列化后发送
 */
export async function sendData(data) {
  return api.send(data)
}

export function useBluetooth() {
  return readonly(state)
}
