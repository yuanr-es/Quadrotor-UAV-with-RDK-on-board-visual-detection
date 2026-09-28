/**
 * 视频会话状态机
 *
 * 状态流转：idle -> detected -> permissionPending -> opening -> streaming -> stopping -> closed
 * 任意状态在异常时可进入 error；closed / error 可回到 idle 重新枚举。
 * 参见 TODOList.md §5.5。
 */

import type { VideoSessionState } from './types'

const TRANSITIONS: Record<VideoSessionState, VideoSessionState[]> = {
  idle: ['detected'],
  detected: ['permissionPending', 'opening', 'idle', 'error'],
  permissionPending: ['opening', 'error', 'idle'],
  opening: ['streaming', 'stopping', 'error'],
  streaming: ['stopping', 'error'],
  stopping: ['closed', 'error'],
  closed: ['idle', 'detected'],
  error: ['idle', 'detected']
}

export type StateChangeListener = (
  to: VideoSessionState,
  from: VideoSessionState
) => void

export interface SessionStateMachine {
  readonly state: VideoSessionState
  canTransition(to: VideoSessionState): boolean
  /** 合法则流转并返回 true；非法返回 false，不改状态 */
  tryTransition(to: VideoSessionState): boolean
  /** 非法流转抛错，用于暴露编排层逻辑错误 */
  transition(to: VideoSessionState): void
  on(listener: StateChangeListener): () => void
  reset(): void
}

export function createSessionStateMachine(
  initial: VideoSessionState = 'idle'
): SessionStateMachine {
  let current: VideoSessionState = initial
  const listeners = new Set<StateChangeListener>()

  const canTransition = (to: VideoSessionState): boolean => {
    if (to === current) return false
    return TRANSITIONS[current].includes(to)
  }

  const tryTransition = (to: VideoSessionState): boolean => {
    if (!canTransition(to)) return false
    const from = current
    current = to
    listeners.forEach((fn) => fn(to, from))
    return true
  }

  return {
    get state() {
      return current
    },
    canTransition,
    tryTransition,
    transition(to) {
      if (!tryTransition(to)) {
        throw new Error(
          `非法视频会话状态流转: ${current} -> ${to}`
        )
      }
    },
    on(listener) {
      listeners.add(listener)
      return () => listeners.delete(listener)
    },
    reset() {
      const from = current
      current = 'idle'
      if (from !== 'idle') listeners.forEach((fn) => fn('idle', from))
    }
  }
}
