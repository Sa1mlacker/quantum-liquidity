/**
 * WebSocket Client with MessagePack
 * Binary protocol for 10x smaller messages
 */

import msgpack from 'msgpack-lite'
import { WSMessage, WSMessageType } from '@/types/api'

const WS_URL = 'ws://localhost:8000/ws/stream'

type MessageHandler = (message: WSMessage) => void
type ErrorHandler = (error: Error) => void

export class WebSocketClient {
  private ws: WebSocket | null = null
  private reconnectTimer: number | null = null
  private reconnectAttempts = 0
  private maxReconnectAttempts = 5
  private reconnectDelay = 1000 // Start with 1 second

  private messageHandlers: Set<MessageHandler> = new Set()
  private errorHandlers: Set<ErrorHandler> = new Set()
  private subscriptions: Set<string> = new Set()

  private _connected = false

  get connected(): boolean {
    return this._connected
  }

  connect(): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      return
    }

    try {
      this.ws = new WebSocket(WS_URL)
      this.ws.binaryType = 'arraybuffer'

      this.ws.onopen = () => {
        console.log('[WS] Connected')
        this._connected = true
        this.reconnectAttempts = 0
        this.reconnectDelay = 1000

        // Resubscribe to topics
        this.subscriptions.forEach((topic) => {
          this.sendMessage({
            type: 'subscribe',
            topic,
          })
        })
      }

      this.ws.onmessage = (event) => {
        try {
          // Decode binary MessagePack
          const buffer = new Uint8Array(event.data)
          const message = msgpack.decode(buffer) as WSMessage

          // Distribute to handlers
          this.messageHandlers.forEach((handler) => {
            try {
              handler(message)
            } catch (error) {
              console.error('[WS] Handler error:', error)
            }
          })
        } catch (error) {
          console.error('[WS] Failed to decode message:', error)
        }
      }

      this.ws.onerror = (event) => {
        console.error('[WS] Error:', event)
        const error = new Error('WebSocket error')
        this.errorHandlers.forEach((handler) => handler(error))
      }

      this.ws.onclose = () => {
        console.log('[WS] Disconnected')
        this._connected = false
        this.scheduleReconnect()
      }
    } catch (error) {
      console.error('[WS] Connection failed:', error)
      this.scheduleReconnect()
    }
  }

  disconnect(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }

    if (this.ws) {
      this.ws.close()
      this.ws = null
    }

    this._connected = false
  }

  private scheduleReconnect(): void {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('[WS] Max reconnection attempts reached')
      return
    }

    this.reconnectAttempts++
    const delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1)

    console.log(`[WS] Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})`)

    this.reconnectTimer = window.setTimeout(() => {
      this.connect()
    }, delay)
  }

  private sendMessage(message: any): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn('[WS] Cannot send message: not connected')
      return
    }

    try {
      // Encode with MessagePack
      const buffer = msgpack.encode(message)
      this.ws.send(buffer)
    } catch (error) {
      console.error('[WS] Failed to send message:', error)
    }
  }

  subscribe(topic: string): void {
    this.subscriptions.add(topic)

    if (this._connected) {
      this.sendMessage({
        type: 'subscribe',
        topic,
      })
    }
  }

  unsubscribe(topic: string): void {
    this.subscriptions.delete(topic)

    if (this._connected) {
      this.sendMessage({
        type: 'unsubscribe',
        topic,
      })
    }
  }

  onMessage(handler: MessageHandler): () => void {
    this.messageHandlers.add(handler)
    return () => this.messageHandlers.delete(handler)
  }

  onError(handler: ErrorHandler): () => void {
    this.errorHandlers.add(handler)
    return () => this.errorHandlers.delete(handler)
  }

  ping(): void {
    this.sendMessage({ type: 'ping' })
  }
}

// Singleton instance
export const wsClient = new WebSocketClient()

// Auto-connect on import
if (typeof window !== 'undefined') {
  wsClient.connect()
}
