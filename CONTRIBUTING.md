# Contribuir

¡Gracias por el interés! Este proyecto es pequeño y cualquier ayuda es bienvenida.

---

## Cómo reportar un problema

Abre un **issue** e incluye:

1. Versión del programa (se ve en la barra de título y en `Acerca de`).
2. Sistema operativo y versión de Windows.
3. Pasos para reproducirlo.
4. Qué esperabas y qué ocurrió.
5. Si aplica, el texto del **registro** inferior (cópialo con `Ctrl+C` tras seleccionarlo).
6. El diagnóstico del ejecutable:

   ```bat
   ConversorWebP.exe --diag "%TEMP%\diag.json"
   ```

   Adjunta `diag.json`.

No adjuntes imágenes personales ni rutas privadas; basta con describir el caso o usar imágenes de prueba.

---

## Cómo proponer cambios

1. Haz un *fork* del repositorio.
2. Crea una rama descriptiva: `feat/peso-objetivo`, `fix/avif-alfa`, `docs/compilar`.
3. Mantén el estilo del código existente:
   - Python con tipos en las firmas públicas.
   - Nombres de funciones y variables en español, comentarios breves.
   - Sin dependencias nuevas salvo que sean imprescindibles; justifícalo en el PR.
4. Comprueba que la sintaxis es correcta y que las pruebas pasan:

   ```bat
   python -m py_compile conversor_webp.py
   python -m unittest discover -s tests -v
   ```

5. Ejecuta la lista de comprobación de [`docs/ARQUITECTURA.md`](docs/ARQUITECTURA.md#11-pruebas).
6. Si el cambio afecta a la interfaz o a las opciones, actualiza `README.md`,
   `MANUAL_DE_USO.html` y `CHANGELOG.md`.
7. Abre el *pull request* explicando el problema, la solución y cómo lo has probado.

---

## Estilo de los commits

Se agradece un mensaje claro en imperativo:

```text
Añade peso objetivo por imagen
Corrige el redondeo de la calidad
Documenta la compilación con PyInstaller
```

---

## Licencia de las contribuciones

Al enviar un *pull request* aceptas que tu aportación se publique bajo la licencia **MIT** del proyecto.
