// uni-app API 浏览器模拟
const uni = {
  showToast({ title, icon }) {
    const el = document.createElement('div')
    el.textContent = title
    Object.assign(el.style, {
      position: 'fixed', bottom: '80px', left: '50%', transform: 'translateX(-50%)',
      background: 'rgba(0,0,0,0.85)', color: '#fff', padding: '12px 24px',
      borderRadius: '8px', fontSize: '14px', zIndex: '9999',
      transition: 'opacity 0.3s', pointerEvents: 'none'
    })
    document.body.appendChild(el)
    setTimeout(() => { el.style.opacity = '0' }, 1500)
    setTimeout(() => { document.body.removeChild(el) }, 2000)
  },

  createSelectorQuery() {
    const self = this
    let selector = null
    const chain = {
      in(comp) { self._comp = comp; return this },
      select(sel) { selector = sel; return this },
      selectAll(sel) { selector = sel; return this },
      boundingClientRect(cb) {
        if (selector) {
          const el = document.querySelector(selector)
          if (el) {
            const rect = el.getBoundingClientRect()
            setTimeout(() => cb([rect]), 0)
          } else {
            setTimeout(() => cb(null), 0)
          }
        }
        return this
      },
      exec() {}
    }
    return chain
  }
}

// 注入为全局变量，模拟 uni-app 的全局 uni
if (typeof window !== 'undefined') {
  window.uni = uni
}

// 也注入为模块，方便 import 使用
export default uni
