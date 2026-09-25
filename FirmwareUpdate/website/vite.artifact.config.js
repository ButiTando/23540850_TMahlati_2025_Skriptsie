import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

/*
 * Builds just our own code into one IIFE, with React left external so the page
 * can load its UMD build from a CDN instead of carrying a second copy. Classic
 * JSX runtime because the automatic one would pull in react/jsx-runtime, which
 * has no UMD global to map to.
 */
export default defineConfig({
  plugins: [react({ jsxRuntime: 'classic' })],
  build: {
    outDir: 'dist-artifact',
    lib: { entry: 'src/artifact-entry.jsx', formats: ['iife'], name: 'IltTestRig',
           fileName: () => 'app.js' },
    rollupOptions: {
      external: ['react', 'react-dom', 'react-dom/client'],
      output: { globals: { react: 'React', 'react-dom': 'ReactDOM',
                           'react-dom/client': 'ReactDOM' } },
    },
    minify: 'esbuild',
  },
});
