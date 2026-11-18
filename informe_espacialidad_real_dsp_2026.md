# Técnicas Científicas y Algoritmos Modernos (2026) para Espacialidad Real 3D, Movimiento de Instrumentos y Preservación de Transientes

## 1. El límite físico del estéreo en banda ancha

Cuando un plugin opera únicamente con procesamiento lineal en banda ancha (matrices Mid/Side o filtros *All-Pass* globales) sobre un bus maestro estéreo de dos canales $(L[n], R[n])$, todos los instrumentos —voz, bajo, guitarras, sintetizadores y batería— están sumados en la misma forma de onda. Como documenta la literatura reciente de *upmixing* ([NJU Journal 2026](https://jns.nju.edu.cn/EN/10.13232/j.cnki.jnju.2026.04.011)), el solapamiento en tiempo y frecuencia impide mover un instrumento individual de izquierda a derecha o en profundidad sin arrastrar y desfasar los demás elementos que comparten esas mismas frecuencias.

Para lograr **espacialidad real** (mover instrumentos, crear profundidad 3D y mantener intactos los transientes y la correlación de fase), la investigación actual en procesamiento digital de señales (DSP) y aprendizaje profundo se divide en cuatro arquitecturas fundamentales.

---

## 2. *Primary-Ambient Extraction* (PAE) en Tiempo-Frecuencia (C++ en tiempo real)

El método estándar en DSP puro para extraer objetos direccionales y ambiente desde un audio estéreo sin usar redes neuronales es **Primary-Ambient Extraction (PAE)** en el dominio STFT o en bandas perceptuales ERB ([He et al., IEEE/ACM TASLP](https://sigport.org/sites/default/files/HeG15_Applying%20Primary%20Ambient%20Extraction%20for%20Immersive%20Spatial%20Audio%20Reproduction-IEEE%20approved.pdf); [Ibrahim, PAE-PCA](https://karimibrahim.com/files/2016-09-01-PAE_PCA.pdf)).

### Modelo matemático
En cada bin o banda perceptual $(k, m)$ de la STFT de entrada $\mathbf{x}(k,m) = [X_L(k,m),\; X_R(k,m)]^T$, la señal se modela como la suma de un componente **primario direccional** $\mathbf{p}$ (fuentes puntuales coherentes) y un componente **ambiental difuso** $\mathbf{a}$ (reverbs y colas no correlacionadas):

$$\mathbf{x}(k,m) = \mathbf{p}(k,m) + \mathbf{a}(k,m)$$

Se estima la **Matriz de Covarianza Espacial (SCM)** $2 \times 2$ suavizada en el tiempo:

$$\mathbf{R}(k,m) = \mathbb{E}\!\left[\mathbf{x}(k,m)\,\mathbf{x}^H(k,m)\right] = \begin{bmatrix} r_{LL} & r_{LR} \\ r_{RL} & r_{RR} \end{bmatrix}$$

Aplicando **Análisis de Componentes Principales (PCA)** sobre $\mathbf{R}(k,m)$ se obtienen sus autovalores $\lambda_1 \ge \lambda_2 \ge 0$ y autovectores $\mathbf{v}_1, \mathbf{v}_2$:

* **Dirección / Paneo del objeto primario:** el autovector dominante $\mathbf{v}_1 = [v_{1L},\; v_{1R}]^T$ captura la fuente direccional principal en esa banda y define su ángulo de paneo $\theta(k,m) = \arctan(|v_{1R}| / |v_{1L}|)$.
* **Extracción de Primario y Ambiente:**
  $$\mathbf{p}(k,m) = \left(\mathbf{v}_1^H \mathbf{x}(k,m)\right)\mathbf{v}_1, \qquad \mathbf{a}(k,m) = \mathbf{x}(k,m) - \mathbf{p}(k,m)$$

**Por qué resuelve tu problema:** Una vez separados $\mathbf{p}$ y $\mathbf{a}$ por frecuencia, puedes dejar intactas las bandas con $\theta \approx 45^\circ$ (voz, bombo y bajo en el centro), **re-panear o proyectar con HRTF únicamente los objetos laterales** ($\mathbf{p}$ con $\theta \neq 45^\circ$) y enviar $\mathbf{a}$ hacia la parte trasera/envolvente sin manchar los transientes centrales.

---

## 3. *Directional Audio Coding* (DirAC)

Propuesto por Ville Pulkki ([DirAC en Spatial Sound Reproduction and Stereo Upmixing](https://www.researchgate.net/publication/237067964_Directional_Audio_Coding_in_Spatial_Sound_Reproduction_and_Stereo_Upmixing)) y extendido recientemente a formatos de orden superior ([HO-DirAC, arXiv 2024](https://arxiv.org/abs/2401.13401)), **DirAC** parametriza el campo sonoro en bandas perceptuales mediante dos magnitudes:

1. **Difusividad $\psi(t, f) \in [0, 1]$:** mide qué fracción de la energía en la banda $(t,f)$ es ambiente difuso frente a sonido directo direccional:
   $$\psi(t,f) = 1 - \frac{\|\mathbb{E}[\mathbf{I}(t,f)]\|}{c\,\mathbb{E}[E(t,f)]}$$
   donde $\mathbf{I}$ es el vector de intensidad acústica activa, $E$ la densidad de energía y $c$ la velocidad del sonido.
2. **Dirección de llegada $\mathbf{D}(t,f)$:** estimada a partir del vector de intensidad promediado con ventana temporal adaptativa por banda (entre $3\text{ ms}$ en agudos y $150\text{ ms}$ en graves para evitar artefactos de modulación rápida).

En la síntesis, la parte direccional $(1 - \psi)$ se renderiza hacia ángulos discretos o HRTF binaurales y la parte difusa $\psi$ se decorrelaciona hacia múltiples direcciones virtuales.

---

## 4. Estado del Arte 2026: Upmixing Asistido por Separación de Fuentes (MSS + PAE)

Los papers de **2025–2026** coinciden en que el PAE puramente analítico tiene errores cuando varios instrumentos coinciden en la misma banda de frecuencia. La solución moderna combina **Music Source Separation (MSS)** con renderizado espacial independiente por *stem*:

* **[S2X-Net (IEEE TASLP, 2026)](https://www.researchgate.net/publication/413816112_S2X-Net_Stereo-to-Multichannel_Neural_Upmixing_Network):** Arquitectura unificada de aprendizaje profundo que resuelve conjuntamente **MSS y PAE** para convertir pistas estéreo a 5.1/7.1 real, modelando claves psicoacústicas de audición espacial humana tanto en música como en cine.
* **[PAEDB (DAFx26, Cambridge/MIT, septiembre 2026)](https://dafx.de/paper-archive/2026/papers/DAFx26_paper_14.pdf):** Pipeline de Skywalker Sound y la Universidad de Miami que combina modelos MSS para aislar voz e instrumentos (*Voice-Primary-Ambient*, VPA) con extracción neural de ambiente estéreo para distribución multicanal.
* **[Stereo Music Upmixing Assisted by Source Separation (2026)](https://jns.nju.edu.cn/EN/10.13232/j.cnki.jnju.2026.04.011):** Propone un esquema de doble ruta donde la mezcla se descompone primero en fuentes constituyentes y se estima su ubicación por separado, reduciendo drásticamente el sesgo por solapamiento tiempo-frecuencia.
* **[Aura — Reward-Tilted Flows (UIUC & Dolby Laboratories, octubre 2026)](https://arxiv.org/html/2610.15967):** Reformula el *upmixing* como un problema inverso generativo con *Flow Matching*, optimizando recompensas espaciales sobre la **Matriz de Covarianza Espacial $\mathbf{R}_b$** en bandas ERB para controlar con precisión la coherencia intercanal (IC) y las diferencias de nivel (ILD) manteniendo consistencia exacta con el audio original.
* **Modelos de Separación de Fuentes de referencia:** [BS-RoFormer](https://www.alphaxiv.org/abs/2309.02612) (espectrograma complejo dividido en 62 subbandas con *Rotary Position Embedding*, `11.99 dB` SDR en MUSDB18HQ), [Mel-Band RoFormer](https://arxiv.org/html/2310.01809v1) (subbandas superpuestas en escala Mel) y [Hybrid Transformers Demucs (HTDemucs)](https://github.com/facebookresearch/demucs).

---

## 5. Renderizado Binaural HRTF, Movimiento L–R y Profundidad Real

Una vez que tienes un objeto o *stem* aislado (por ejemplo, desde PAE o en un bus individual del DAW), moverlo en espacio 3D real requiere modelar las tres claves de localización humana ([3D Tune-In Toolkit, PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/); [MDPI HRTF Review 2024](https://www.mdpi.com/2076-3417/14/23/11242)):

1. **ITD (*Interaural Time Difference*) desacoplado de la convolución:**
   Para evitar artefactos al mover fuentes en tiempo real, el retardo interaural se extrae de la HRIR y se aplica aparte mediante una línea de retardo fraccional usando el modelo esférico de Woodworth:
   $$\text{ITD}(\theta) \approx \frac{a}{c}\bigl(\theta + \sin\theta\bigr), \qquad a \approx 0.0875\text{ m},\; c \approx 343\text{ m/s}$$
2. **ILD y Filtros Pinna (HRTF con pre-procesamiento de fase):**
   Interpolación baricéntrica de HRIRs en formato SOFA con convolución particionada (UPOLS) y **pre-procesamiento de fase HRTF** ([Gunnarsson, Uppsala 2024](https://uu.diva-portal.org/smash/get/diva2:1856435/FULLTEXT01.pdf)) para reducir la variabilidad espacial de fase en altas frecuencias y evitar coloración por filtro peine.
3. **Corrección de Campo Cercano (*Near-Field*) y Distancia:**
   Simulación de distancia mediante filtros biquad dependientes de $(d, \theta_I)$ con *cross-fading* de coeficientes entre tramas ([3D Tune-In Toolkit](https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/)), combinada con la relación directo-a-reverberante y reflexiones tempranas (*Image Source Method*).

---

## 6. Cancelación de Diafonía (*Cross-Talk Cancellation* / BACCH) para Monitores

Al escuchar en altavoces estéreo, el altavoz izquierdo alcanza también el oído derecho (diafonía), colapsando la escena a los $\pm 30^\circ$ físicos. La cancelación de diafonía ([BACCH / Choueiri](https://static1.squarespace.com/static/5ceefd9ab0fb290001f9aeb2/t/6316ae0ac8c3c32ed0c01da0/1662430742764/MNhhcN-BACCH-Labs-Intellectual-Property-Details.pdf); [Geometrical Acoustics CTC](https://www.researchgate.net/publication/362847171_Geometrical_Acoustics_Approach_to_Crosstalk_Cancellation); [Beamforming CTC](https://www.chohner.com/_astro/hohnerlein2016_ma_thesis_beamforming_CTC.BMSbsL-S.pdf)) invierte la matriz de transferencia acústica $2 \times 2$ altavoz-oído $\mathbf{H}(\omega)$ con regularización dependiente de la frecuencia $\beta(\omega)$:

$$\mathbf{W}(\omega) = \left(\mathbf{H}^H(\omega)\mathbf{H}(\omega) + \beta(\omega)\mathbf{I}\right)^{-1}\mathbf{H}^H(\omega)$$

Esto permite reproducir audio binaural a través de dos monitores de estudio con separación de canales suficiente para proyectar objetos virtuales hasta $\pm 120^\circ$ alrededor del oyente.

---

## 7. Comparativa de Viabilidad para tu Plugin VST3 / CLAP

| Enfoque | Movimiento real L–R y 3D | Preservación de transientes | Latencia típica | Viabilidad en C++ (VST3/CLAP) |
| :--- | :--- | :--- | :--- | :--- |
| **M/S + All-Pass en Master** (actual) | Nulo (afecta a toda la mezcla junta) | Baja si se empuja fuerte | `0 ms` | Muy simple, pero inadecuado para mover objetos |
| **[PAE (PCA en STFT)](https://sigport.org/sites/default/files/HeG15_Applying%20Primary%20Ambient%20Extraction%20for%20Immersive%20Spatial%20Audio%20Reproduction-IEEE%20approved.pdf) + [HRTF](https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/)** | **Medio–Alto** (separa objetos por ángulo y ambiente) | **Alta** (el centro directo queda intacto) | `10–23 ms` (ventana FFT) | **Óptimo en C++ puro** sin modelos de IA pesados |
| **[DirAC](https://www.researchgate.net/publication/237067964_Directional_Audio_Coding_in_Spatial_Sound_Reproduction_and_Stereo_Upmixing) en bandas ERB** | **Alto** (síntesis direccional vs. difusa) | **Alta** (control temporal por banda) | `10–20 ms` | Alta; requiere banco de filtros + estimador de difusividad |
| **[MSS (BS-RoFormer / HTDemucs)](https://www.alphaxiv.org/abs/2309.02612) + [S2X-Net](https://www.researchgate.net/publication/413816112_S2X-Net_Stereo-to-Multichannel_Neural_Upmixing_Network)** | **Máximo** (control total por instrumento) | **Muy Alta** (batería/bajo aislados) | `> 100 ms` o procesado offline | Requiere inferencia ONNX/GPU; inviable a baja latencia en CPU básica |
| **[HRTF Panner + Near-Field](https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/) en Buses del DAW** | **Total** (objetos 3D tipo Dolby Atmos) | **100 % intacta** | `0–5 ms` | **Ideal para mezcla en FL Studio** (aplicado por pista/bus) |

---

## 8. Conclusión y Ruta Recomendada

1. **Si el plugin va en el Master Bus de canciones ya mezcladas:** abandona el M/S en banda ancha e implementa **PAE mediante PCA en STFT (bandas ERB) + Renderizado HRTF** ([He et al.](https://sigport.org/sites/default/files/HeG15_Applying%20Primary%20Ambient%20Extraction%20for%20Immersive%20Spatial%20Audio%20Reproduction-IEEE%20approved.pdf), [3DTI](https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/)). Así proteges el centro ($\theta \approx 45^\circ$, voz/bombo/bajo), amplías solo los objetos laterales y colocas el ambiente detrás con fase controlada.
2. **Si buscas calidad de mezcla profesional en tus propios proyectos de FL Studio:** convierte el plugin en un **Panner Binaural 3D + Early Reflections + Bajo Armónico** para insertarlo en **pistas y buses individuales** (sintes, coros, guitarras), dejando el bus de batería y subgrave intactos.

---

## Referencias

1. He, J., Tan, E.-L., & Gan, W.-S., *Applying Primary Ambient Extraction for Immersive Spatial Audio Reproduction*, IEEE/ACM Transactions on Audio, Speech, and Language Processing, [https://sigport.org/sites/default/files/HeG15_Applying%20Primary%20Ambient%20Extraction%20for%20Immersive%20Spatial%20Audio%20Reproduction-IEEE%20approved.pdf](https://sigport.org/sites/default/files/HeG15_Applying%20Primary%20Ambient%20Extraction%20for%20Immersive%20Spatial%20Audio%20Reproduction-IEEE%20approved.pdf).
2. Ibrahim, K., *Primary-Ambient Extraction in Audio Signals Using PCA*, [https://karimibrahim.com/files/2016-09-01-PAE_PCA.pdf](https://karimibrahim.com/files/2016-09-01-PAE_PCA.pdf).
3. Pulkki, V., *Directional Audio Coding in Spatial Sound Reproduction and Stereo Upmixing*, AES 28th International Conference, [https://www.researchgate.net/publication/237067964_Directional_Audio_Coding_in_Spatial_Sound_Reproduction_and_Stereo_Upmixing](https://www.researchgate.net/publication/237067964_Directional_Audio_Coding_in_Spatial_Sound_Reproduction_and_Stereo_Upmixing).
4. Hold, C., McCormack, L., Politis, A., & Pulkki, V., *Perceptually-motivated Spatial Audio Codec for Higher-Order Ambisonics Compression (HO-DirAC)*, arXiv:2401.13401, [https://arxiv.org/abs/2401.13401](https://arxiv.org/abs/2401.13401).
5. *S2X-Net: Stereo-to-Multichannel Neural Upmixing Network*, IEEE Transactions on Audio, Speech and Language Processing 34:4362-4375 (2026), [https://www.researchgate.net/publication/413816112_S2X-Net_Stereo-to-Multichannel_Neural_Upmixing_Network](https://www.researchgate.net/publication/413816112_S2X-Net_Stereo-to-Multichannel_Neural_Upmixing_Network).
6. Tong, N. N., & Collins, T., *PAEDB: A Synthetic Primary-Ambient Dataset Generation Pipeline for Automatic Upmixing Using Deep Neural Networks*, DAFx26, MIT (septiembre de 2026), [https://dafx.de/paper-archive/2026/papers/DAFx26_paper_14.pdf](https://dafx.de/paper-archive/2026/papers/DAFx26_paper_14.pdf).
7. *Stereo Music Upmixing Assisted by Source Separation*, Journal of Nanjing University (2026), [https://jns.nju.edu.cn/EN/10.13232/j.cnki.jnju.2026.04.011](https://jns.nju.edu.cn/EN/10.13232/j.cnki.jnju.2026.04.011).
8. Basu, S., Bhattacharya, G., & Seefeldt, A. (UIUC & Dolby Laboratories), *Controllable Mono-to-Stereo Upmixing using Reward-tilted Flows (Aura)*, arXiv:2610.15967 (octubre de 2026), [https://arxiv.org/html/2610.15967](https://arxiv.org/html/2610.15967).
9. Lu, W., et al., *Music Source Separation with Band-Split RoPE Transformer (BS-RoFormer)*, [https://www.alphaxiv.org/abs/2309.02612](https://www.alphaxiv.org/abs/2309.02612).
10. Wang, J.-C., et al., *Mel-Band RoFormer for Music Source Separation*, arXiv:2310.01809, [https://arxiv.org/html/2310.01809v1](https://arxiv.org/html/2310.01809v1).
11. Cuevas-Rodríguez, M., et al., *3D Tune-In Toolkit: An open-source library for real-time binaural spatialisation*, PMC6411112, [https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/](https://pmc.ncbi.nlm.nih.gov/articles/PMC6411112/).
12. Gunnarsson, V., *Binaural Modeling for High-Fidelity Spatial Audio (HRTF Phase Pre-Processing)*, Uppsala University (2024), [https://uu.diva-portal.org/smash/get/diva2:1856435/FULLTEXT01.pdf](https://uu.diva-portal.org/smash/get/diva2:1856435/FULLTEXT01.pdf).
13. *A Review on Head-Related Transfer Function Generation for Spatial Audio*, MDPI Applied Sciences (2024), [https://www.mdpi.com/2076-3417/14/23/11242](https://www.mdpi.com/2076-3417/14/23/11242).
14. Choueiri, E. Y., *Optimal Crosstalk Cancellation for Binaural Audio (BACCH Labs IP Details)*, [https://static1.squarespace.com/static/5ceefd9ab0fb290001f9aeb2/t/6316ae0ac8c3c32ed0c01da0/1662430742764/MNhhcN-BACCH-Labs-Intellectual-Property-Details.pdf](https://static1.squarespace.com/static/5ceefd9ab0fb290001f9aeb2/t/6316ae0ac8c3c32ed0c01da0/1662430742764/MNhhcN-BACCH-Labs-Intellectual-Property-Details.pdf).
15. *Geometrical Acoustics Approach to Crosstalk Cancellation*, ResearchGate, [https://www.researchgate.net/publication/362847171_Geometrical_Acoustics_Approach_to_Crosstalk_Cancellation](https://www.researchgate.net/publication/362847171_Geometrical_Acoustics_Approach_to_Crosstalk_Cancellation).
16. Hohnerlein, C., *Beamforming-based Acoustic Crosstalk Cancelation for Spatial Audio (RACE)*, [https://www.chohner.com/_astro/hohnerlein2016_ma_thesis_beamforming_CTC.BMSbsL-S.pdf](https://www.chohner.com/_astro/hohnerlein2016_ma_thesis_beamforming_CTC.BMSbsL-S.pdf).
