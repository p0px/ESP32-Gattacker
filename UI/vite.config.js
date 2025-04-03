import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react-swc'

// https://vitejs.dev/config/
export default defineConfig({
  server: {
    port: 9001,
  },
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks: undefined
      }
    },
  },
})
