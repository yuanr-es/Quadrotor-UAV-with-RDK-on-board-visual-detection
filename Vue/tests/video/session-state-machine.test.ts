import { describe, it, expect, vi } from 'vitest'
import { createSessionStateMachine } from '../../domain/video/session-state-machine'

describe('视频会话状态机', () => {
  it('正常流程 idle -> ... -> closed', () => {
    const m = createSessionStateMachine()
    expect(m.tryTransition('detected')).toBe(true)
    expect(m.tryTransition('permissionPending')).toBe(true)
    expect(m.tryTransition('opening')).toBe(true)
    expect(m.tryTransition('streaming')).toBe(true)
    expect(m.tryTransition('stopping')).toBe(true)
    expect(m.tryTransition('closed')).toBe(true)
    expect(m.state).toBe('closed')
  })

  it('拒绝非法流转且不改变状态', () => {
    const m = createSessionStateMachine()
    expect(m.tryTransition('streaming')).toBe(false)
    expect(m.state).toBe('idle')
    expect(() => m.transition('streaming')).toThrow()
  })

  it('重复关闭是幂等的（不抛错）', () => {
    const m = createSessionStateMachine()
    m.tryTransition('detected')
    m.tryTransition('opening')
    m.tryTransition('stopping')
    m.tryTransition('closed')
    expect(m.tryTransition('closed')).toBe(false)
    expect(m.state).toBe('closed')
  })

  it('拔出/异常可进入 error 并可回到 idle', () => {
    const m = createSessionStateMachine()
    m.tryTransition('detected')
    m.tryTransition('opening')
    m.tryTransition('streaming')
    expect(m.tryTransition('error')).toBe(true)
    expect(m.tryTransition('idle')).toBe(true)
  })

  it('监听器收到 from/to', () => {
    const m = createSessionStateMachine()
    const spy = vi.fn()
    m.on(spy)
    m.tryTransition('detected')
    expect(spy).toHaveBeenCalledWith('detected', 'idle')
  })

  it('reset 回到 idle', () => {
    const m = createSessionStateMachine('streaming')
    m.reset()
    expect(m.state).toBe('idle')
  })
})
