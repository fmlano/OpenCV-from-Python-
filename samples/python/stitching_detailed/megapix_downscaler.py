from .megapix_scaler import MegapixScaler


class MegapixDownscaler(MegapixScaler):

    @staticmethod
    def force_downscale(scale):
        return min(1.0, scale)

    def _set_scale(self, scale):
        scale = self.force_downscale(scale)
        super()._set_scale(scale)
